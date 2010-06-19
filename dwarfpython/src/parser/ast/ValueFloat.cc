#include "ast.h"
/**
 * inherits: BasePhrase
 * private: double f;
 */

ValueFloat *ValueFloat::parse(T_FLOAT f)
{
    ValueFloat *vf = new ValueFloat();
    vf->f = f;
    return vf;
}
void ValueFloat::toStream(std::ostream& strm)
{
    strm << this->f;
}
val ValueFloat::evaluate(ParathonContext& c)
{
	return (val) { true, { i_double: f }, p_builtin_double_type };
}
