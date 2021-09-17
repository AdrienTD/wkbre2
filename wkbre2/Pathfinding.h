#pragma once

#include <map>
#include <set>
#include <limits>
//#include <iostream>

namespace Pathfinding {

    struct PFPos {
        int x, z;
        bool operator==(const PFPos& p) const { return x == p.x && z == p.z; }
        bool operator!=(const PFPos& p) const { return !(*this == p); }
        bool operator<(const PFPos& p) const { return (x != p.x) ? (x < p.x) : (z < p.z); }
    };

    inline int ManhattanDiagHeuristic(PFPos o, PFPos n) {
        int dx = std::abs(n.x - o.x);
        int dz = std::abs(n.z - o.z);
        int ndiag = std::min(dx, dz);
        int nrect = std::max(dx, dz) - ndiag;
        return ndiag * 141 + nrect * 100;
    };

    inline int EuclideanHeuristic(PFPos o, PFPos n) {
        return (int)(std::sqrt((float)(n.x - o.x) * (n.x - o.x) + (float)(n.z - o.z) * (n.z - o.z)) * 100.0f);
    }

    template<typename Predicate, typename Heuristic>
    std::vector<PFPos> DoPathfinding(PFPos start, PFPos end, Predicate pred, Heuristic heuristic) {
        using score_t = int;
        std::set<PFPos> nextTiles;
        std::set<PFPos> visited;
        std::map<PFPos, int> scores;
        std::map<PFPos, PFPos> edges;

        nextTiles.insert(start);
        scores[start] = 0;

        while (true) {
            // find tile with min score
            PFPos mt;
            score_t mscore = std::numeric_limits<score_t>::max();
            for (auto& nt : nextTiles) {
                if (!visited.count(nt)) {
                    score_t tscore = scores[nt] + heuristic(nt, end);
                    if (tscore < mscore) {
                        mscore = tscore;
                        mt = nt;
                    }
                }
            }
            if (mscore == std::numeric_limits<score_t>::max()) {
                //std::cout << "FUCK!\n";
                return {};
            }
            //printf("%i %i\n", mt.x, mt.z);
            if (mt == end) {
                //std::cout << "END REACHED!\n";
                break;
            }

            // update neighbours
            auto updateNeighbour = [mt, &nextTiles, &scores, &pred, &visited, &edges](int dx, int dz, score_t cost) {
                PFPos pfp{ mt.x + dx, mt.z + dz };
                if (!pred(pfp)) {
                    score_t newscore = scores.at(mt) + cost;
                    if (!scores.count(pfp) || newscore < scores.at(pfp)) {
                        scores[pfp] = newscore;
                        edges[pfp] = mt;
                        if (!visited.count(pfp))
                            nextTiles.insert(pfp);
                    }
                }
            };
            updateNeighbour(1, 0, 100);
            updateNeighbour(-1, 0, 100);
            updateNeighbour(0, 1, 100);
            updateNeighbour(0, -1, 100);
            updateNeighbour(1, 1, 141);
            updateNeighbour(-1, 1, 141);
            updateNeighbour(1, -1, 141);
            updateNeighbour(-1, -1, 141);
            visited.insert(mt);
            nextTiles.erase(mt);
        }

        std::vector<PFPos> vec;
        PFPos pfp = end;
        while (pfp != start) {
            vec.push_back(pfp);
            pfp = edges.at(pfp);
        }
        vec.push_back(start);
        return vec;
    }

};