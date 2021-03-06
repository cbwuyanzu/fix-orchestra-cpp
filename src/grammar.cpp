#include <grammar.h>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/support.hpp>

namespace score {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phx = boost::phoenix;

template <typename Iterator, typename Skipper>
struct Grammar : qi::grammar<Iterator, ast::Statement(), Skipper> {
  Grammar() : Grammar::base_type(statementRule) {
    using namespace qi;
    using namespace boost::gregorian;
    using namespace boost::posix_time;

    statementRule = (assignmentRule | exprRule) >> eoi;

    assignmentRule = varRule >> lit('=') >> exprRule;

    exprRule      = orRule.alias();
    orRule
      = andRule[_val = _1]
        >> *(((lit("or") | lit("||")) >> andRule) [_val = phx::construct<ast::BinaryRel<ast::RelOr>>(_val, _1)]);
    andRule
      = eqRule[_val = _1]
        >> *(((lit("and") | lit("&&")) >> eqRule) [_val = phx::construct<ast::BinaryRel<ast::RelAnd>>(_val, _1)]);
    eqRule
      = inclusionRule[_val = _1]
        >> *( ((lit("eq") | lit("==")) >> inclusionRule) [_val = phx::construct<ast::BinaryRel<ast::RelEq>>(_val, _1)]
            | ((lit("ne") | lit("!=")) >> inclusionRule) [_val = phx::construct<ast::BinaryRel<ast::RelNotEq>>(_val, _1)]);
    inclusionRule // TODO: does this take precedence over `relRule`s? we should think about this
      = relRule[_val = _1]
        >> *( (lit("between") >> lit("[") >> relRule >> lit(",") >> relRule >> lit("]")) [_val = phx::construct<ast::Range>(_val, _1, _2)] // XXX: differs from ANTLR grammar!
            | (lit("in") >> lit("{") >> (relRule % ',') >> lit("}")) [_val = phx::construct<ast::Contains>(_val, _1)]);
    relRule
      = addSubRule[_val = _1]
        >> *( ((lit("<") | lit("lt")) >> addSubRule) [_val = phx::construct<ast::BinaryRel<ast::RelLessThan>>(_val, _1)]
            | ((lit("<=") | lit("le")) >> addSubRule) [_val = phx::construct<ast::BinaryRel<ast::RelLessThanEq>>(_val, _1)]
            | ((lit(">") | lit("gt")) >> addSubRule) [_val = phx::construct<ast::BinaryRel<ast::RelGreaterThan>>(_val, _1)]
            | ((lit(">=") | lit("ge")) >> addSubRule) [_val = phx::construct<ast::BinaryRel<ast::RelGreaterThanEq>>(_val, _1)]);
    addSubRule
      = mulDivRule[_val = _1]
        >> *( (lit('+') >> mulDivRule) [_val = phx::construct<ast::BinaryOp<ast::OpAdd>>(_val, _1)]
            | (lit('-') >> mulDivRule) [_val = phx::construct<ast::BinaryOp<ast::OpSub>>(_val, _1)]);
    mulDivRule
      = unaryRule[_val = _1]
        >> *( (lit('*') >> unaryRule) [_val = phx::construct<ast::BinaryOp<ast::OpMult>>(_val, _1)]
            | (lit('/') >> unaryRule) [_val = phx::construct<ast::BinaryOp<ast::OpDiv>>(_val, _1)]
            | ((lit("mod") | lit("%")) >> unaryRule) [_val = phx::construct<ast::BinaryOp<ast::OpMod>>(_val, _1)]);
    unaryRule // TODO: better to use '>' instead of '>>' ... but throws exception on bad parse
      = (lit('-') >> exprRule) [_val = phx::construct<ast::UnaryOp<ast::OpUnaryMinus>>(_1)]
      | (lit('!') >> exprRule) [_val = phx::construct<ast::UnaryOp<ast::OpLogicalNot>>(_1)]
      | simpleRule            [_val = _1];
    simpleRule
      %= lit('(') >> exprRule >> lit(')')
      |  lit('#') >> datetimeRule >> lit('#')
      |  lit('#') >> timeRule >> lit('#')
      |  lit('#') >> dateRule >> lit('#')
      |  double_ // TODO: allow negative numbers? replace with a parser for fixed-precision
      |  uint_   // had to switch order with double_ (c.f. ANTLR grammar)
      |  lit('\'') >> stringCharRule >> lit('\'')
      |  lit('\"') >> +stringCharRule >> lit('\"')
      |  existsRule
      |  varRule;

    existsRule = lit("exists") >> varRule [_val = phx::construct<ast::Exists>(_1)]; // TODO: consider using '>'

    predRule %= idRule >> lit("==") >> exprRule;
    qualRule %= idRule >> -(lit('[') >> (uint_  | predRule) >> lit(']'));
    varRule   = (-(string("$")|string("^")|string("in.")|string("out.")|string("this."))
                  >> ((qualRule % '.'))) [_val = phx::construct<ast::Variable>(_1, _2)];

    datetimeRule %= dateRule >> timeRule;
    dateRule     %= uint_parser<unsigned short, 10, 4, 4>()
                    >> lit('-') >> uint_parser<unsigned char, 10, 2, 2>()
                    >> lit('-') >> uint_parser<unsigned char, 10, 2, 2>();
    timeRule      =      (lit('T') >> uint_parser<unsigned short, 10, 2, 2>())
                         [_val += phx::construct<hours>(_1)]
                      >> (lit(':') >> uint_parser<unsigned char, 10, 2, 2>())
                         [_val += phx::construct<minutes>(_1)]
                      >> -(lit(':') >> uint_parser<unsigned char, 10, 2, 2>()
                          [_val += phx::construct<seconds>(_1)])
                      >> -(lit('.') >> ulong_
                          [_val += phx::construct<milliseconds>(_1)]);

    idRule = alpha >> *(alnum | char_('_'));

    stringCharRule = ~char_("\\\"")
                   | lit('\\') >> char_("btnfr\"'\\");

    BOOST_SPIRIT_DEBUG_NODE(statementRule);

    BOOST_SPIRIT_DEBUG_NODE(assignmentRule);

    BOOST_SPIRIT_DEBUG_NODE(exprRule);

    BOOST_SPIRIT_DEBUG_NODE(orRule);
    BOOST_SPIRIT_DEBUG_NODE(andRule);
    BOOST_SPIRIT_DEBUG_NODE(eqRule);
    BOOST_SPIRIT_DEBUG_NODE(inclusionRule);
    BOOST_SPIRIT_DEBUG_NODE(relRule);
    BOOST_SPIRIT_DEBUG_NODE(addSubRule);
    BOOST_SPIRIT_DEBUG_NODE(mulDivRule);
    BOOST_SPIRIT_DEBUG_NODE(unaryRule);
    BOOST_SPIRIT_DEBUG_NODE(simpleRule);
    BOOST_SPIRIT_DEBUG_NODE(existsRule);
    BOOST_SPIRIT_DEBUG_NODE(varRule);

    BOOST_SPIRIT_DEBUG_NODE(datetimeRule);
    BOOST_SPIRIT_DEBUG_NODE(timeRule);
    BOOST_SPIRIT_DEBUG_NODE(dateRule);

    BOOST_SPIRIT_DEBUG_NODE(predRule);
    BOOST_SPIRIT_DEBUG_NODE(qualRule);
    BOOST_SPIRIT_DEBUG_NODE(varRule);

    BOOST_SPIRIT_DEBUG_NODE(idRule);
    BOOST_SPIRIT_DEBUG_NODE(stringCharRule);
  }

  qi::rule<Iterator, ast::Statement(),  Skipper> statementRule;

  qi::rule<Iterator, ast::Assignment(), Skipper> assignmentRule;

  qi::rule<Iterator, ast::Expr(),       Skipper> exprRule,
                                                 orRule,
                                                 andRule,
                                                 eqRule,
                                                 inclusionRule,
                                                 relRule,
                                                 addSubRule,
                                                 mulDivRule,
                                                 unaryRule,
                                                 simpleRule,
                                                 existsRule;

  qi::rule<Iterator, boost::posix_time::time_duration(), Skipper> timeRule;
  qi::rule<Iterator, boost::gregorian::date(),           Skipper> dateRule;
  qi::rule<Iterator, boost::posix_time::ptime(),         Skipper> datetimeRule;

  qi::rule<Iterator, ast::Predicate(), Skipper> predRule;
  qi::rule<Iterator, ast::Qualifier(), Skipper> qualRule;
  qi::rule<Iterator, ast::Variable(),  Skipper> varRule;

  qi::rule<Iterator, std::string(),    Skipper> idRule;
  qi::rule<Iterator, char(),           Skipper> stringCharRule;
};

bool parse(ast::Statement &out, const std::string &in) {
  // skipper approach adapted from: https://stackoverflow.com/a/44531686
  using Iterator = std::string::const_iterator;

  qi::rule<Iterator> lineComment, blockComment, skipper;
  {
    using namespace qi;

    lineComment  = lit("//") >> *(char_ - eol) >> (eol|eoi);
    blockComment = lit("/*") >> *(blockComment | char_ - lit("*/")) > lit("*/");
    skipper      = space | lineComment | blockComment;
  }

  score::Grammar<Iterator, decltype(skipper)> grammar;

  const auto &iter = in.cbegin();
  const auto &end = in.cend();
  bool parsed = phrase_parse(iter, end, grammar, skipper, out);

  return parsed;
}
}
