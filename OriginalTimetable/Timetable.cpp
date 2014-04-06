#include <string>
#include <thread>
#include <future>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random> // for testing
#include <set>
#include <chrono>

#include "../Lib/List.h"
#include "../Lib/RBMap.h"
#include "../Lib/RBTree.h"

// Original translation from Simon Marlow's book

using Talk = int;

struct Person
{
    std::string _name;
    List<Talk>  _talks;
};

std::ostream& operator<<(std::ostream& os, Person const & p)
{
    os << p._name << ", " << p._talks << std::endl;
    return os;
}

using Persons = List<Person>;

using TalkList = List<Talk>;

using TalkSet = RBTree<Talk>;

using TimeTable = List<TalkList>;

using Selections = List<std::pair<Talk, List<Talk>>>;

template<class T>
List<std::pair<T, List<T>>> selAccum(List<T> const & leftLst, List<T> const & rightLst)
{
    if (rightLst.isEmpty())
        return List<std::pair<T, List<T>>>();
    else
    {
        // make recursive call moving one element from right to left list
        auto rest = selAccum(leftLst.push_front(rightLst.front()), rightLst.pop_front());
        // pair head of right list with left ++ right.tail
        auto p = std::make_pair(rightLst.front(), concat(leftLst, rightLst.pop_front()));
        // prepend pair to rest
        return rest.push_front(p);
    }
}

// Create list of pairs: (element, rest of elements)
template<class T>
List<std::pair<T, List<T>>> selects(List<T> const & lst)
{
    return selAccum(List<T>(), lst);
}

// Constraints for the solution search

struct Constr
{
    Constr(int maxSlots, int maxTracks, List<Person> const & people)
        : _maxSlots(maxSlots), _maxTracks(maxTracks)
    {
        forEach(people, [&](Person const & person)
        {
            Selections sels = selects(person._talks);
            sels.forEach([&](std::pair<Talk, List<Talk>> const & p)
            {
                TalkSet set(std::begin(p.second), std::end(p.second));
                _clashMap = _clashMap.insertWith(p.first, set, &treeUnion<Talk>);
            });
        });
    }
    bool isMaxTracks(int trackNo) const { return trackNo == _maxTracks; }
    bool isMaxSlots(int curSlotNo) const { return curSlotNo == _maxSlots; }
    TalkSet clashesWith(Talk t) const 
    { 
        return _clashMap.findWithDefault(TalkSet(), t); 
    }

private:
    int _maxSlots;
    int _maxTracks;
    RBMap<Talk, TalkSet> _clashMap;
};

// Partially filled slot

struct PartSlot
{
    PartSlot(TalkList const & allTalks)
        : _curTrackNo(0), _talksForSlot(allTalks)
    {}
    PartSlot( int curTrackNo
            , TalkList const & talksInSlot
            , TalkList const & remainingTalks)
        : _curTrackNo(curTrackNo)
        , _talksInSlot(talksInSlot)
        , _talksForSlot(remainingTalks)
    {}
    bool isFinished(Constr const & constr) const
    {
        return constr.isMaxTracks(_curTrackNo);
    }
    // List of pairs: 
    // (talk that's been allocated, partially filled slot with that talk)
    List<std::pair<Talk, PartSlot>> refine(Constr const & constr) const;

    int       _curTrackNo;
    TalkList  _talksInSlot;
    TalkList  _talksForSlot;
};

// Partial Solution

struct PartSol
{
    typedef TimeTable SolutionT;

    PartSol(TalkList const & allTalks)
        : _curSlotNo(0)
        , _partSlot(allTalks)
        , _remainingTalks(allTalks)
    {}
    PartSol(int curSlotNo
           , TimeTable const & tableSoFar
           , TalkList const & remainingTalks)
        : _curSlotNo(curSlotNo)
        , _partSlot(remainingTalks)
        , _tableSoFar(tableSoFar)
        , _remainingTalks(remainingTalks)
    {}
    PartSol( int curSlotNo
           , PartSlot partSlot
           , TimeTable const & tableSoFar
           , TalkList const & remainingTalks)
        : _curSlotNo(curSlotNo)
        , _partSlot(partSlot)
        , _tableSoFar(tableSoFar)
        , _remainingTalks(remainingTalks)
    {}
    bool isFinished(Constr const & constr) const
    {
        return constr.isMaxSlots(_curSlotNo);
    }
    List<PartSol> refine(Constr const & constr) const;
    TimeTable getSolution() const { return _tableSoFar; }
    friend std::ostream& operator<<(std::ostream& os, PartSol const & p);
private:
    int       _curSlotNo;
    PartSlot  _partSlot;
    TalkList  _remainingTalks;
    TimeTable _tableSoFar;
};

std::ostream& operator<<(std::ostream& os, PartSol const & p)
{
    os << p._curSlotNo << ", " << p._partSlot._curTrackNo << "| in: " 
       << p._partSlot._talksInSlot << ", for: " << p._partSlot._talksForSlot 
       << ", left: " << p._remainingTalks << std::endl;
    return os;
}

List<std::pair<Talk, PartSlot>> PartSlot::refine(Constr const & constr) const
{
    List<std::pair<Talk, List<Talk>>> pairs = selects(_talksForSlot);

    List<std::pair<Talk, PartSlot>> candts;
    pairs.forEach([&](std::pair<Talk, List<Talk>> const & p) {
        TalkSet clashesWithT = constr.clashesWith(p.first);
        candts = candts.push_front(
            std::make_pair(
                p.first,
                PartSlot( _curTrackNo + 1
                        , _talksInSlot.push_front(p.first)
                        , diff(p.second, clashesWithT))));
    });
    return candts;
}

List<PartSol> PartSol::refine(Constr const & constr) const
{
    List<PartSol> candts;
    if (_partSlot.isFinished(constr))
    {
        candts = candts.push_front(
            PartSol( _curSlotNo + 1
                   , _tableSoFar.push_front(_partSlot._talksInSlot)
                   , _remainingTalks));
    }
    else
    {
        List<std::pair<Talk, PartSlot>> partSlots = _partSlot.refine(constr);
        forEach(std::move(partSlots), [this, &candts](std::pair<Talk, PartSlot> const & partSlot)
        {
            candts = candts.push_front(
                PartSol(_curSlotNo
                       , partSlot.second
                       , _tableSoFar
                       , _remainingTalks.remove1(partSlot.first)));

        });
    }
    return candts;
}

template<class Partial, class Constraint>
std::vector<typename Partial::SolutionT> generate(Partial const & part, Constraint const & constr)
{
    using SolutionVec = std::vector<typename Partial::SolutionT>;
    if (part.isFinished(constr))
    {
        SolutionVec candts{ part.getSolution() };
        return candts;
    }
    else
    {
        SolutionVec candts;
        List<Partial> partList = part.refine(constr);
        partList.forEach([&](Partial const & part){
            std::vector<typename Partial::SolutionT> lst = generate(part, constr);
            candts.reserve(candts.size() + lst.size());
            std::copy(lst.begin(), lst.end(), std::back_inserter(candts));
        });
        return candts;
    }
}

template<class T>
std::vector<T> when_all_vec(std::vector<std::future<T>> & ftrs)
{
    std::vector<T> lst;
    while (!ftrs.empty())
    {
        std::future<T> f = std::move(ftrs.back());
        lst.push_back(f.get());
        ftrs.pop_back();
    }
    return lst;
}

template<class T>
std::vector<T> concatAll(std::vector<std::vector<T>> const & in)
{
    unsigned total = std::accumulate(in.begin(), in.end(), 0u,
        [](unsigned sum, std::vector<T> const & v) { return sum + v.size(); });
    std::vector<T> res;
    res.reserve(total);
    std::for_each(in.begin(), in.end(), [&res](std::vector<T> const & v){
        std::copy(v.begin(), v.end(), std::back_inserter(res));
    });
    return res;
}

template<class Partial, class Constraint>
std::vector<typename Partial::SolutionT> generatePar(int depth, Partial const & part, Constraint const & constr)
{
    using SolutionVec = std::vector<typename Partial::SolutionT>;

    if (depth == 0)
    {
        return generate(part, constr);
    }
    else if (part.isFinished(constr))
    {
        SolutionVec candts { part.getSolution() };
        return candts;
    }
    else
    {
        std::vector<std::future<SolutionVec>> futResult;
        List<Partial> partList = part.refine(constr);
        partList.forEach([&constr, &futResult, depth](Partial const & part)
        {
            std::future<SolutionVec> futLst =
                std::async([constr, part, depth]() {
                    return generatePar(depth - 1, part, constr);
                });
            futResult.push_back(std::move(futLst));
        });
        std::vector<SolutionVec> all = when_all_vec(futResult);
        return concatAll(all);
    }
}

std::vector<TimeTable> timeTable(Persons const & persons, TalkList const & allTalks, int maxTracks, int maxSlots, bool isPar)
{
    Constr constr(maxSlots, maxTracks, persons);
    PartSol emptySol(allTalks);
    if (isPar)
        return generatePar(3, emptySol, constr);
    else
        return generate(emptySol, constr);
}

std::vector<TimeTable> test()
{
    List<Talk> l1 = { 1, 2 };
    List<Talk> l2 = { 2, 3 };
    List<Talk> l3 = { 3, 4 };
    List<Person> persons = { { "P", l1 }, { "Q", l2 }, { "R", l3 } };
    List<Talk> talks = { 1, 2, 3, 4 };
    return timeTable(persons, talks, 2, 2, false);
}

#if 0
int randTalks(int nTalks, int maxTalk, std::default_random_engine & rand)
{
    std::uniform_int_distribution<int> dist(0, maxTalk - 1);
    std::set<int> talkSet;
    while (nTalks > 0)
    {
        int talk = dist(rand);
        if (talkSet.find(talk) == talkSet.end())
        {
            talkSet.insert(talk);
            --nTalks;
        }
    }
    return fromIt(talkSet.begin(), talkSet.end());
}
#endif

std::vector<TimeTable> bench(bool isPar)
{
    List<Talk> talks = iterateN([](int i) { return i + 1; }, 1, 12);
    List<Person> persons = {
        { "P10", { 8, 9, 2 } },
        { "P9", { 4, 3, 6 } },
        { "P8", { 11, 9, 10 } },
        { "P7", { 8, 5, 3 } },
        { "P6", { 2, 10, 5 } },
        { "P5", { 7, 3, 8 } },
        { "P4", { 9, 6, 10 } },
        { "P3", { 8, 1, 6 } },
        { "P2", { 3, 8, 4 } },
        { "P1", { 10, 8, 6 } }
    };
    //std::cout << persons << std::endl;
    return timeTable(persons, talks, 4, 3, isPar);
}

void main()
{
    std::vector<TimeTable> table = test();
    for_each(table.begin(), table.end(), [](TimeTable const & table){
        forEach(table, [](TalkList const & talks){
            std::cout << talks;
        });
        std::cout << std::endl;
    });
    std::cout << table.size() << " solutions\n";
}