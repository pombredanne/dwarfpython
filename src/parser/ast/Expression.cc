#include "ast.h"
/**
 * inherits: ComparisonPhrase
 * protected: Operator *op;
 * virtual: val evaluate();
 */
Expression *Expression::parse(XorExpression *xe)
{
    return xe;
}
Expression *Expression::parse(XorExpression *lhs, OpBor *op, Expression *rhs)
{
    Expression *e = new Expression();
    e->lhs = lhs;
    e->op = op;
    e->rhs = rhs;
    return e;
}
void Expression::toStream(std::ostream& strm)
{
    strm << "(" << this->lhs << " " << this->op << " " << this->rhs << ")";
}

