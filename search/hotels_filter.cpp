#include "search/hotels_filter.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/assert.hpp"

#include "std/algorithm.hpp"

namespace search
{
namespace hotels_filter
{
namespace
{
void CompileLogicOp(shared_ptr<Rule> const & lhs, shared_ptr<Rule> const & rhs, bool whenTrue,
                    Program & program)

{
  if (!lhs && !rhs)
  {
    program.emplace_back(Instruction::Const{!whenTrue});
    return;
  }

  if (!lhs)
  {
    rhs->Compile(program);
    return;
  }

  lhs->Compile(program);
  if (!rhs)
    return;

  program.emplace_back(Instruction::Jump{0 /* offset */, whenTrue});
  size_t const i = program.size();
  rhs->Compile(program);
  program[i - 1].m_jump.m_offset = program.size() - i;
}
}  // namespace

// static
typename Rating::Value const Rating::kDefault = 0;

// static
typename PriceRate::Value const PriceRate::kDefault = 0;

// Description -------------------------------------------------------------------------------------
void Description::FromFeature(FeatureType & ft)
{
  m_rating = Rating::kDefault;
  m_priceRate = PriceRate::kDefault;

  auto const & metadata = ft.GetMetadata();

  if (metadata.Has(feature::Metadata::FMD_RATING))
  {
    string const rating = metadata.Get(feature::Metadata::FMD_RATING);
    float r;
    if (strings::to_float(rating, r))
      m_rating = r;
  }

  if (metadata.Has(feature::Metadata::FMD_PRICE_RATE))
  {
    string const priceRate = metadata.Get(feature::Metadata::FMD_PRICE_RATE);
    int pr;
    if (strings::to_int(priceRate, pr))
      m_priceRate = pr;
  }

  m_types = ftypes::IsHotelChecker::Instance().GetHotelTypesMask(ft);
}

// Rule --------------------------------------------------------------------------------------------
// static
bool Rule::IsIdentical(shared_ptr<Rule> const & lhs, shared_ptr<Rule> const & rhs)
{
  if (lhs && !rhs)
    return false;
  if (!lhs && rhs)
    return false;

  if (lhs && rhs && !lhs->IdenticalTo(*rhs))
    return false;

  return true;
}

string DebugPrint(Rule const & rule) { return rule.ToString(); }

// HotelsFilter::AndRule ---------------------------------------------------------------------------
void AndRule::Compile(Program & program) const
{
  CompileLogicOp(m_lhs, m_rhs, false /* whenTrue */, program);
}

// HotelsFilter::OrRule ----------------------------------------------------------------------------
void OrRule::Compile(Program & program) const
{
  CompileLogicOp(m_lhs, m_rhs, true /* whenTrue */, program);
}

// HotelsFilter::ScopedFilter ----------------------------------------------------------------------
HotelsFilter::ScopedFilter::ScopedFilter(MwmSet::MwmId const & mwmId,
                                         Descriptions const & descriptions, Rule const & rule)
  : m_mwmId(mwmId), m_descriptions(descriptions), m_vm(rule)
{
}

bool HotelsFilter::ScopedFilter::Matches(FeatureID const & fid) const
{
  if (fid.m_mwmId != m_mwmId)
    return false;

  auto it =
      lower_bound(m_descriptions.begin(), m_descriptions.end(),
                  make_pair(fid.m_index, Description{}),
                  [](pair<uint32_t, Description> const & lhs,
                     pair<uint32_t, Description> const & rhs) { return lhs.first < rhs.first; });
  if (it == m_descriptions.end() || it->first != fid.m_index)
    return false;

  return m_vm.Run(it->second);
}

// VM ----------------------------------------------------------------------------------------------
VM::VM(Rule const & rule) { rule.Compile(m_program); }

bool VM::Run(Description const & description) const
{
  Context ctx;

  while (ctx.m_rip < m_program.size())
  {
    auto const & instruction = m_program[ctx.m_rip++];
    Eval(description, instruction, ctx);
  }

  ASSERT_EQUAL(ctx.m_rip, m_program.size(), ());
  return ctx.m_rax;
}

template <typename Field>
bool VM::EvalBinaryOp(Instruction::Comparision op, Description const & description,
                      typename Field::Value rhs) const
{
  auto const lhs = Field::Select(description);
  switch (op)
  {
  case Instruction::Comparision::Lt: return Field::Lt(lhs, rhs);
  case Instruction::Comparision::Le: return Field::Lt(lhs, rhs) || Field::Eq(lhs, rhs);
  case Instruction::Comparision::Eq: return Field::Eq(lhs, rhs);
  case Instruction::Comparision::Ge: return Field::Gt(lhs, rhs) || Field::Eq(lhs, rhs);
  case Instruction::Comparision::Gt: return Field::Gt(lhs, rhs);
  }
}

void VM::Eval(Description const & description, Instruction const & instruction, Context & ctx) const
{
  switch (instruction.m_type)
  {
  case Instruction::Type::CompareRating:
  {
    auto const & args = instruction.m_compareRating;
    ctx.m_rax = EvalBinaryOp<Rating>(args.m_type, description, args.m_rating);
    return;
  }
  case Instruction::Type::ComparePrice:
  {
    auto const & args = instruction.m_comparePrice;
    ctx.m_rax = EvalBinaryOp<PriceRate>(args.m_type, description, args.m_price);
    return;
  }
  case Instruction::Type::CheckType:
  {
    auto const & args = instruction.m_checkType;
    ctx.m_rax = (description.m_types & args.m_types) != 0;
    return;
  }
  case Instruction::Type::Jump:
  {
    auto const & args = instruction.m_jump;
    if (ctx.m_rax == args.m_whenTrue)
      ctx.m_rip += args.m_offset;
    return;
  }
  case Instruction::Type::Const:
  {
    ctx.m_rax = instruction.m_const.m_value;
    return;
  }
  }
}

// HotelsFilter ------------------------------------------------------------------------------------
HotelsFilter::HotelsFilter(HotelsCache & hotels): m_hotels(hotels) {}

unique_ptr<HotelsFilter::ScopedFilter> HotelsFilter::MakeScopedFilter(MwmContext const & context,
                                                                      shared_ptr<Rule> rule)
{
  if (!rule)
    return {};
  return make_unique<ScopedFilter>(context.GetId(), GetDescriptions(context), *rule);
}

void HotelsFilter::ClearCaches()
{
  m_descriptions.clear();
}

HotelsFilter::Descriptions const & HotelsFilter::GetDescriptions(MwmContext const & context)
{
  auto const & mwmId = context.GetId();
  auto const it = m_descriptions.find(mwmId);
  if (it != m_descriptions.end())
    return it->second;

  auto const hotels = m_hotels.Get(context);
  auto & descriptions = m_descriptions[mwmId];
  hotels.ForEach([&descriptions, &context](uint32_t id) {
    FeatureType ft;

    Description description;
    if (context.GetFeature(id, ft))
      description.FromFeature(ft);
    descriptions.emplace_back(id, description);
  });
  return descriptions;
}
}  // namespace hotels_filter
}  // namespace search
