#include "ast.h"

/**
 * Abstract class for individual statements.
 *
 * Each subclass (parsed below) inherits from this.
 */
SmallStatement *SmallStatement::parse(PrintStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(ImportStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(DeleteStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(BreakStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(ContinueStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(PassStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(GlobalStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(AssignStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(TestStatement *s)
{
    return s;
}
SmallStatement *SmallStatement::parse(ReturnStatement *s)
{
    return s;
}
ParathonValue *SmallStatement::evaluate(ParathonContext& c)
{
    return NULL;
}
void SmallStatement::evaluate_print(ParathonContext& c, std::ostream& strm)
{
    strm << this->evaluate(c) << std::endl;
}
