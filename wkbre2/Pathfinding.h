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
        size_t hash() const { return (size_t)x + (size_t)(z * 3); }
        struct Hasher {
            size_t operator()(const PFPos& pfp) const noexcept { return pfp.hash(); }
        };
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

    struct AStarPathfinder {
        using score_t = int;
        //using pfp_set = std::set<PFPos>;
        //template<typename T> using pfp_map = std::map<PFPos, T>;
        using pfp_set = std::unordered_set<PFPos, PFPos::Hasher>;
        template<typename T> using pfp_map = std::unordered_map<PFPos, T, PFPos::Hasher>;
        std::vector<std::pair<PFPos, score_t>> nextTiles;
        pfp_set visited;
        pfp_map<int> scores;
        pfp_map<PFPos> edges;
        PFPos start, end;
        bool finished = false;
        bool nothingFound = false;

        void begin(PFPos start, PFPos end) {
            visited.clear();
            scores.clear();
            edges.clear();
            this->start = start;
            this->end = end;
            finished = false;
            nothingFound = false;

            scores[start] = 0;
            nextTiles = { {start, 0} };
        }

        template<typename Predicate, typename Heuristic>
        bool next(Predicate pred, Heuristic heuristic) {
            if (finished) return true;

            static auto heapcmp = [](const auto& a, const auto& b) {return a.second > b.second; };

            // find tile with min score
            PFPos mt;
            score_t mscore = std::numeric_limits<score_t>::max();
            while (!nextTiles.empty()) {
                std::tie(mt, mscore) = nextTiles.front();
                std::pop_heap(nextTiles.begin(), nextTiles.end(), heapcmp);
                nextTiles.pop_back();
                if (!visited.count(mt))
                    break;
            }
            if (mscore == std::numeric_limits<score_t>::max()) {
                //std::cout << "FUCK!\n";
                finished = true;
                nothingFound = true;
                return true;
            }
            //printf("%i %i\n", mt.x, mt.z);
            if (mt == end) {
                //std::cout << "END REACHED!\n";
                finished = true;
                nothingFound = false;
                return true;
            }

            // update neighbours
            auto updateNeighbour = [this, mt, &pred, &heuristic](int dx, int dz, score_t cost) {
                PFPos pfp{ mt.x + dx, mt.z + dz };
                if (!pred(pfp)) {
                    score_t newscore = scores.at(mt) + cost;
                    if (!scores.count(pfp) || newscore < scores.at(pfp)) {
                        scores[pfp] = newscore;
                        edges[pfp] = mt;
                        if (!visited.count(pfp)) {
                            nextTiles.emplace_back(pfp, newscore + heuristic(pfp, end));
                            std::push_heap(nextTiles.begin(), nextTiles.end(), heapcmp);
                        }
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
            return false;
        }

        std::vector<PFPos> get() {
            if (nothingFound)
                return {};
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

    template<typename Predicate, typename Heuristic>
    std::vector<PFPos> DoPathfinding(PFPos start, PFPos end, Predicate pred, Heuristic heuristic) {
        //AStarPathfinder astar;
        //astar.begin(start, end);

        //while (!astar.next(pred, heuristic));

        //return astar.get();

        AStarPathfinder astar1, astar2;
        astar1.begin(start, end);
        astar2.begin(end, start);

        bool stop = false;
        while (true) {
            if (astar1.next(pred, heuristic)) return astar1.get();
            if (astar2.next(pred, heuristic)) {
                auto vec = astar2.get();
                std::reverse(vec.begin(), vec.end());
                return vec;
            }
        }
    }
};