#ifndef SEARCH_DUMP_HUIFASDHUIASDFA
#define SEARCH_DUMP_HUIFASDHUIASDFA

#include <memory>
#include <vector>

class AnyVarRef;

struct SearchDumper
{
    virtual void output_node(long long nodeCount, const std::vector<AnyVarRef>& vars, bool isSolution) = 0;  
    virtual void backtrack() = 0;
    virtual void branch(const std::string& varname, DomainInt val, bool isLeft) = 0;
    virtual ~SearchDumper() {}
};


std::shared_ptr<SearchDumper> makeDumpTreeJson(std::ostream* o);

#endif