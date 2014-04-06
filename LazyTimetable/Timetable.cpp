#include "../Lib/List.h"
#include "../Lib/RBMap.h"
#include "../Lib/RBTree.h"
#include "../Lib/PureStream.h"
#include "../Lib/Futures.h"

#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <future>
#include <chrono>

// Lazy Backtracking 

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

// Constraints for the solution search

struct Constr
{
    Constr(int maxSlots, int maxTracks, List<Person> const & people)
        : _maxSlots(maxSlots), _maxTracks(maxTracks)
    {
        forEach(people, [&](Person const & person)
        {
            TalkList talks = person._talks;
            forEach(talks, [this, talks](Talk tk)
            {
                TalkList otherTalks = talks.remove(tk);
                TalkSet set(std::begin(otherTalks), std::end(otherTalks));
                _clashMap = _clashMap.insertWith(tk, set, &treeUnion<Talk>);

            });
        });
    }
    bool isMaxTracks(int trackNo) const { return trackNo == _maxTracks; }
    bool isMaxSlots(int slotNo) const { return slotNo == _maxSlots; }
    TalkSet clashesWith(Talk t) const
    {
        return _clashMap.findWithDefault(TalkSet(), t);
    }

private:
    int _maxSlots;
    int _maxTracks;
    RBMap<Talk, TalkSet> _clashMap;
};

struct PartSol
{
    PartSol() {} // needed by Susp
    PartSol(TalkList const & talks)
    : _curSlotNo(0), _remainingTalks(talks)
    {}
    PartSol(int curSlotNo, TalkList const & remainingTalks, TimeTable tableSoFar)
    : _curSlotNo(curSlotNo), _remainingTalks(remainingTalks), _tableSoFar(tableSoFar)
    {}
    PartSol removeTalk(Talk tk) const
    {
        return PartSol(_curSlotNo, _remainingTalks.remove1(tk), _tableSoFar);
    }
    PartSol fillSlot(TalkList const & talks) const
    {
        return PartSol(_curSlotNo + 1, _remainingTalks, _tableSoFar.push_front(talks));
    }
    int _curSlotNo;
    TalkList  _remainingTalks;
    TimeTable _tableSoFar;
};

struct PartSlot
{
    typedef TimeTable SolutionT;

    PartSlot() {} // needed by Susp
    PartSlot(PartSol const & partSol)
    : _curTrackNo(0), _talksForSlot(partSol._remainingTalks), _partSol(partSol)
    {}
    PartSlot(int curTrackNo, TalkList const & talksInSlot, TalkList const & talksForSlot, PartSol const & partSol)
    : _curTrackNo(curTrackNo), _talksInSlot(talksInSlot), _talksForSlot(talksForSlot), _partSol(partSol)
    {}
    // Return lazy streams
    Stream<PartSlot> refineSlot(TalkList candts, Constr const & constr) const;
    Stream<PartSlot> refine(Constr const & constr) const;
    bool isFinished(Constr const & constr) const
    {
        return constr.isMaxSlots(_partSol._curSlotNo);
    }
    TimeTable getSolution() const { return _partSol._tableSoFar;  }

    int       _curTrackNo;
    TalkList  _talksInSlot;
    TalkList  _talksForSlot;
    PartSol   _partSol;
};

Stream<PartSlot> PartSlot::refineSlot(TalkList candts, Constr const & constr) const
{
    if (candts.isEmpty())
        return Stream<PartSlot>();
    return Stream<PartSlot>([candts, this, &constr]() -> Cell<PartSlot>
    {
        Talk tk = candts.front();
        TalkList otherTalks = _talksForSlot.remove1(tk);
        TalkSet clashesWithT = constr.clashesWith(tk);
        PartSlot partSlot(_curTrackNo + 1
            , _talksInSlot.push_front(tk)
            , diff(otherTalks, clashesWithT)
            , _partSol.removeTalk(tk));
        Stream<PartSlot> tailStream = refineSlot(candts.pop_front(), constr);
        return Cell<PartSlot>(partSlot, tailStream);
    });
}

Stream<PartSlot> PartSlot::refine(Constr const & constr) const
{
    if (constr.isMaxTracks(_curTrackNo))
    {
        return Stream<PartSlot>(PartSlot(_partSol.fillSlot(_talksInSlot)));
    }
    else
    {
        return refineSlot(_talksForSlot, constr);
    }
}

template<class Partial, class Constraint>
int generate(Partial const & part, Constraint const & constr)
{
    if (part.isFinished(constr))
    {
        return 1;
    }
    else
    {
        int candts = 0;
        Stream<Partial> partList = part.refine(constr);
        forEach(std::move(partList), [&](Partial const & part){
            candts += generate(part, constr);
        });
        return candts;
    }
}

template<class Partial, class Constraint>
int generatePar(int depth, Partial const & part, Constraint const & constr)
{
    if (depth == 0)
    {
        return generate(part, constr);
    }
    else if (part.isFinished(constr))
    {
        return 1;
    }
    else
    {
        std::vector<std::future<int>> futResult;
        Stream<Partial> partList = part.refine(constr);
        forEach(std::move(partList), [&constr, &futResult, depth](Partial const & part)
        {
            std::future<int> futCount =
                std::async([constr, part, depth]() {
                return generatePar(depth - 1, part, constr);
            });
            futResult.push_back(std::move(futCount));
        });
        List<int> all = when_all_vec(futResult);
        int candts = 0;
        forEach(std::move(all), [&candts](int i) { candts += i; });
        return candts;
    }
}

int timeTable(Persons const & persons, TalkList const & allTalks, int maxTracks, int maxSlots, bool isPar = false)
{
    Constr constr(maxSlots, maxTracks, persons);
    PartSol emptySol(allTalks);
    if (isPar)
        return generatePar(2, PartSlot(emptySol), constr);
    else
        return generate(PartSlot(emptySol), constr);
}

int test()
{
    List<Talk> l1 = { 1, 2 };
    List<Talk> l2 = { 2, 3 };
    List<Talk> l3 = { 3, 4 };
    List<Person> persons = { { "P", l1 }, { "Q", l2 }, { "R", l3 } };
    List<Talk> talks = { 1, 2, 3, 4 };
    return timeTable(persons, talks, 2, 2);
}

int bench(bool isPar)
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

void testBench(bool isPar = false)
{
    auto start = std::chrono::steady_clock::now();
    int solCount = bench(isPar);
    auto end = std::chrono::steady_clock::now();
    std::cout << "Found " << solCount << " solutions" << std::endl;
    auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    std::cout << diff_sec.count() << std::endl;
}

void main()
{
    std::cout << "Lazy algorithm\n";
    //std::cout << test() << std::endl;

    std::cout << "Parallel\n";
    testBench(true);
    std::cout << "Sequential\n";
    testBench();
    std::cout << "Parallel\n";
    testBench(true);
    std::cout << "Sequential\n";
    testBench();
}
