#include "MessageFilter.h"

#include <QCoreApplication>

void MessageFilter::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool MessageFilter::IsEnabled() const
{
    return m_enabled;
}

void MessageFilter::AddRule(const FilterRule &rule)
{
    m_rules.append(rule);
}

void MessageFilter::RemoveRule(int row)
{
    if (row >= 0 && row < m_rules.size()) {
        m_rules.removeAt(row);
    }
}

void MessageFilter::SetRuleEnabled(int row, bool enabled)
{
    if (row >= 0 && row < m_rules.size()) {
        m_rules[row].enabled = enabled;
    }
}

void MessageFilter::ClearRules()
{
    m_rules.clear();
}

const QVector<FilterRule> &MessageFilter::Rules() const
{
    return m_rules;
}

bool MessageFilter::Accepts(const CanFrame &frame) const
{
    if (!m_enabled) {
        return true;
    }

    bool hasIncludeRule = false;
    bool included = false;
    for (const FilterRule &rule : m_rules) {
        if (!rule.enabled) {
            continue;
        }
        const bool matches = RuleMatches(rule, frame.id);
        if (rule.include) {
            hasIncludeRule = true;
            included = included || matches;
        } else if (matches) {
            return false;
        }
    }
    return !hasIncludeRule || included;
}

bool MessageFilter::RuleMatches(const FilterRule &rule, quint32 id)
{
    switch (rule.mode) {
    case FilterMode::SingleId:
        return id == rule.first;
    case FilterMode::IdRange:
        return id >= qMin(rule.first, rule.second) && id <= qMax(rule.first, rule.second);
    case FilterMode::Mask:
        return (id & rule.second) == (rule.first & rule.second);
    }
    return false;
}

QString MessageFilter::DescribeRule(const FilterRule &rule)
{
    const QString first = QString::number(rule.first, 16).toUpper();
    const QString second = QString::number(rule.second, 16).toUpper();
    switch (rule.mode) {
    case FilterMode::SingleId:
        return first;
    case FilterMode::IdRange:
        return QStringLiteral("%1..%2").arg(first, second);
    case FilterMode::Mask:
        return QCoreApplication::translate("MessageFilter", "%1 / maske %2").arg(first, second);
    }
    return QString();
}

QString MessageFilter::ModeName(FilterMode mode)
{
    switch (mode) {
    case FilterMode::SingleId:
        return QCoreApplication::translate("MessageFilter", "Tek ID");
    case FilterMode::IdRange:
        return QCoreApplication::translate("MessageFilter", "Aralık");
    case FilterMode::Mask:
        return QCoreApplication::translate("MessageFilter", "Maske");
    }
    return QString();
}
