/* -------------------------------------------------
 * Header
 */

#include "cache.h"

#include <map>
#include <vector>

using namespace std;

#define ull uint64_t
#define uint uint32_t
#define OCCUPANCY_LEN 128
#define MAXVAL 31
#define PCT_SIZE 2048
#define MAXRRIP 7
#define HIST_ENTRIES 2800
#define HIST_LEN 8
#define HIST_SETS (HIST_ENTRIES/HIST_LEN)
#define MAXTIME 1024


#define bitmask(l) (((l) == 64)? (ull)(-1LL) : ((1LL<<(l)) - 1LL))
#define bits(x, i, l) (((x) >> (i)) & bitmask(l))
#define SAMPLED(set) (bits(set, 0 , 6) == bits(set, (LLC_SETL - 6), 6) )

/* -------------------------------------------------
 * Helper functions
 */
 
ull CRC(ull address);            
ull log2(uint x);

const ull LLC_SETL = (ull)log2(LLC_SET);

struct HISTORY {
    ull PC;
    uint last_time, lru;
    
    void init() {
        PC = lru = last_time = 0;
    }

    void update(uint current_time, ull PC) {
        last_time = current_time;
        PC = PC;
    }
};

/* -------------------------------------------------
 * OPTgen
 */

class OPTgen {
private:
    vector<uint> occupancy;
    ull cache_size;
public:
    void init(ull size);
    void access(ull val);
    bool generate(ull slot, ull last_slot);
} optgen[LLC_SET];


/* -------------------------------------------------
 * Predictor
 */


class Predictor {
private:
    map<ull, int> PCT;
public:
    bool predict(ull PC);
    void enhance(ull PC);
    void detrain(ull PC);
} *predictor;


/* -------------------------------------------------
 * Replacement Strategy
 */

uint rrip[LLC_SET][LLC_WAY];
ull PCs[LLC_SET][LLC_WAY];
ull timer[LLC_SET];

vector<map<ull, HISTORY> > history;



void CACHE::llc_initialize_replacement() {
    cout << LLC_SETL << endl;
    for (int i=0; i<LLC_SET; i++) {
        for (int j=0; j<LLC_WAY; j++) {
            rrip[i][j] = MAXRRIP;
            PCs[i][j] = 0;
        }
        timer[i] = 0;
        optgen[i].init(LLC_WAY - 2);
    }

    history.resize(HIST_SETS);
    for (int i=0; i<HIST_SETS; i++) {
        history[i].clear();
    }
    predictor = new Predictor();
}


uint CACHE::llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type) {

    for (uint i=0; i<LLC_WAY; i++) {
        if (rrip[set][i] == MAXRRIP) {
            return i;
        }
    }
    int maxv= 0;
    int victim = -1;
    for (int i=0; i<LLC_WAY; i++) {
        if (rrip[set][i] >= maxv) {
            maxv = rrip[set][i];
            victim = i;
        }
    }
    if (SAMPLED(set)) {
        predictor->detrain(PCs[set][victim]);
    }
    return victim;

}


void update_lru(uint sample_set, uint current_time);
void erase_lru(uint sample_set);
void train(uint set, ull full_addr, ull ip); 

void CACHE::llc_update_replacement_state(unsigned int cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    full_addr = (full_addr >> 6) << 6;
    PCs[set][way] = ip;

    if (type == WRITEBACK || type == PREFETCH) {
        return;
    }

    train(set, full_addr, ip);

    bool prediction = predictor->predict(ip);

    if (!prediction) {
        rrip[set][way] = MAXRRIP;
    } else {
        rrip[set][way] = 0;
        if (!hit) {
            bool full = false;
            for(int i=0; i<LLC_WAY; i++) {
                if (rrip[set][i] == MAXRRIP-1) {
                    full = true;
                }
            }
            if (!full) {
                for (int i=0; i<LLC_WAY; i++) {
                    rrip[set][i] ++ ;
                }
            }
        }
        rrip[set][way] = 0;
    }
}

void CACHE::llc_replacement_final_stats() { }


/* -------------------------------------------------
 * Function implements
 */


ull CRC(ull address) {
    ull crcPolynomial = 3988292384ULL;
    ull result = address;
    for (uint i=0; i<32; i++) {
        if ((result & 1) == 1) {
            result = (result >> 1) ^ crcPolynomial;
        } else {
            result >>= 1;
        }
    }
    return result;
}
ull log2(uint x) {
    ull ret = 0;
    for (x>>=1; x; x>>=1, ret+=1);
    return ret;
}

void train(uint set, ull full_addr, ull ip) {
    if (SAMPLED(set)) {
        ull slot = timer[set] % OCCUPANCY_LEN;
        ull sample_tag = CRC(full_addr >> 12) % 256;
        uint sample_set = (full_addr >> 6) % HIST_SETS;
        uint current_time = timer[set];

        if (history[sample_set].find(sample_tag) != history[sample_set].end()) {
            uint last_time = history[sample_set][sample_tag].last_time;
            ull last_slot = last_time % OCCUPANCY_LEN;
            if (current_time < last_time) {
                current_time += MAXTIME;
            }
            bool tooFar = (current_time - last_time) > OCCUPANCY_LEN;

            if (!tooFar && optgen[set].generate(slot, last_slot)) {
                predictor->enhance(history[sample_set][sample_tag].PC);
            } else {
                predictor->detrain(history[sample_set][sample_tag].PC);
            }
            update_lru(sample_set, history[sample_set][sample_tag].lru);
        } else {
            if (history[sample_set].size() == HIST_LEN) {
                erase_lru(sample_set);
            }
            history[sample_set][sample_tag].init();
            update_lru(sample_set, HIST_LEN-1);
        }

        optgen[set].access(slot);
        history[sample_set][sample_tag].update(timer[set], ip);
        history[sample_set][sample_tag].lru = 0;
        timer[set] = (timer[set] + 1) % MAXTIME;
    }
}

void update_lru(uint sample_set, uint lru) {
    for (map<ull, HISTORY>::iterator it = history[sample_set].begin(); it != history[sample_set].end(); it ++) {
        if ((it->second).lru < lru) {
            (it->second).lru ++ ;
        }
    }
}

void erase_lru(uint sample_set) {
    ull addr = 0;
    for (map<ull, HISTORY>::iterator it = history[sample_set].begin(); it != history[sample_set].end(); it++) {
        if ((it->second).lru == (HIST_LEN-1)) {
            addr = it->first;
            break;
        }
    }
    history[sample_set].erase(addr);
}

bool Predictor::predict(ull PC) {
    ull key = CRC(PC) % PCT_SIZE;
    if (PCT.find(key) != PCT.end() && PCT[key] < ((MAXVAL+1) / 2)) {
        return false;
    }
    else return true;
}
void Predictor::enhance(ull PC) {
    ull key = CRC(PC) % PCT_SIZE;
    if (PCT.find(key) == PCT.end()) {
        PCT[key] = (MAXVAL+1) >> 1;
    } 
    PCT[key] = min(MAXVAL, PCT[key] + 1);
}

void Predictor::detrain(ull PC) {
    ull key = CRC(PC) % PCT_SIZE;
    if(PCT.find(key) == PCT.end()){
        PCT[key] = (MAXVAL + 1) >> 1;
    }
    PCT[key] = max(0, PCT[key] - 1);
}

void OPTgen::init(ull size) {
    cache_size = size;
    occupancy.resize(OCCUPANCY_LEN, 0);
}
void OPTgen::access(ull val) {
    occupancy[val] = 0;
}
bool OPTgen::generate(ull slot, ull last_slot) {
    bool cache = true;
    for (uint i=last_slot; i!=slot; i=(i+1)%OCCUPANCY_LEN) {
        if (occupancy[i] >= cache_size) {
            cache = false;
            break;
        }
    }
    if (cache) {
        for (uint i=last_slot; i!=slot; i=(i+1)%OCCUPANCY_LEN) {
            occupancy[i] += 1;
        }
    }
    return cache;
}
