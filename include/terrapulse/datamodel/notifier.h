#pragma once

#include "terrapulse/core/message.h"
#include "terrapulse/datamodel/object.h"

#include <QString>

#include <memory>
#include <vector>

namespace tp::datamodel {

class Notifier : public tp::core::BaseObject {
public:
    Notifier() = default;
    Notifier(QString parentID, Operation operation, std::shared_ptr<Object> object);

    const char* className() const override { return "DataModel::Notifier"; }

    QString parentID() const { return m_parentID; }
    Operation operation() const { return m_operation; }
    std::shared_ptr<Object> object() const { return m_object; }

    static void SetEnabled(bool enabled);
    static bool IsEnabled();
    static void Clear();
    static std::size_t Size();
    static void Create(const QString& parentID, Operation operation, std::shared_ptr<Object> object);
    static std::vector<Notifier> Take();

private:
    QString m_parentID;
    Operation m_operation = Operation::Undefined;
    std::shared_ptr<Object> m_object;
};

class NotifierMessage : public tp::core::Message {
public:
    const char* className() const override { return "DataModel::NotifierMessage"; }
    void addNotifier(const Notifier& notifier);
    const std::vector<Notifier>& notifiers() const { return m_notifiers; }

private:
    std::vector<Notifier> m_notifiers;
};

} // namespace tp::datamodel
