#include "terrapulse/datamodel/dataavailability.h"
#include "terrapulse/datamodel/event.h"
#include "terrapulse/datamodel/inventory.h"
#include "terrapulse/datamodel/notifier.h"
#include "terrapulse/datamodel/qualitycontrol.h"
#include "terrapulse/datamodel/strongmotion/strongmotionparameters.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <atomic>

namespace tp::datamodel {
namespace {

QMutex& registryMutex() {
    static QMutex mutex;
    return mutex;
}

QHash<QString, PublicObject*>& registry() {
    static QHash<QString, PublicObject*> objects;
    return objects;
}

std::atomic_bool& registrationEnabled() {
    static std::atomic_bool enabled{true};
    return enabled;
}

QMutex& notifierMutex() {
    static QMutex mutex;
    return mutex;
}

std::vector<Notifier>& notifierPool() {
    static std::vector<Notifier> pool;
    return pool;
}

std::atomic_bool& notifierEnabled() {
    static std::atomic_bool enabled{true};
    return enabled;
}

std::atomic_uint64_t& idCounter() {
    static std::atomic_uint64_t counter{1};
    return counter;
}

} // namespace

std::unique_ptr<tp::core::BaseObject> Object::clone() const {
    return cloneObject();
}

bool Object::setParent(PublicObject* parent) {
    m_parent = parent;
    return true;
}

bool Object::assign(const Object& other) {
    m_lastModifiedInArchive = other.m_lastModifiedInArchive;
    return true;
}

std::unique_ptr<Object> Object::cloneObject() const {
    return std::make_unique<Object>(*this);
}

PublicObject::PublicObject(QString publicID)
    : m_publicID(std::move(publicID)) {
    registerMe();
}

PublicObject::PublicObject(const PublicObject& other)
    : Object(other), m_publicID(other.m_publicID), m_registered(false) {}

PublicObject& PublicObject::operator=(const PublicObject& other) {
    if (this == &other) {
        return *this;
    }
    deregisterMe();
    Object::assign(other);
    m_publicID = other.m_publicID;
    m_registered = false;
    return *this;
}

PublicObject::~PublicObject() {
    deregisterMe();
}

std::unique_ptr<Object> PublicObject::cloneObject() const {
    return std::make_unique<PublicObject>(*this);
}

bool PublicObject::setPublicID(QString publicID) {
    if (m_registered) {
        return false;
    }
    m_publicID = std::move(publicID);
    return true;
}

bool PublicObject::registerMe() {
    if (m_registered || m_publicID.isEmpty() || !registrationEnabled().load()) {
        return false;
    }

    QMutexLocker locker(&registryMutex());
    auto& objects = registry();
    if (objects.contains(m_publicID)) {
        return false;
    }
    objects.insert(m_publicID, this);
    m_registered = true;
    return true;
}

void PublicObject::deregisterMe() {
    if (!m_registered) {
        return;
    }

    QMutexLocker locker(&registryMutex());
    registry().remove(m_publicID);
    m_registered = false;
}

PublicObject* PublicObject::Find(const QString& publicID) {
    QMutexLocker locker(&registryMutex());
    return registry().value(publicID, nullptr);
}

std::size_t PublicObject::ObjectCount() {
    QMutexLocker locker(&registryMutex());
    return static_cast<std::size_t>(registry().size());
}

void PublicObject::SetRegistrationEnabled(bool enabled) {
    registrationEnabled().store(enabled);
}

bool PublicObject::RegistrationEnabled() {
    return registrationEnabled().load();
}

QString PublicObject::GenerateId(const QString& prefix) {
    return QString("%1/%2").arg(prefix).arg(idCounter().fetch_add(1));
}

Notifier::Notifier(QString parentID, Operation operation, std::shared_ptr<Object> object)
    : m_parentID(std::move(parentID)), m_operation(operation), m_object(std::move(object)) {}

void Notifier::SetEnabled(bool enabled) {
    notifierEnabled().store(enabled);
}

bool Notifier::IsEnabled() {
    return notifierEnabled().load();
}

void Notifier::Clear() {
    QMutexLocker locker(&notifierMutex());
    notifierPool().clear();
}

std::size_t Notifier::Size() {
    QMutexLocker locker(&notifierMutex());
    return notifierPool().size();
}

void Notifier::Create(const QString& parentID, Operation operation, std::shared_ptr<Object> object) {
    if (!notifierEnabled().load() || !object) {
        return;
    }

    QMutexLocker locker(&notifierMutex());
    notifierPool().emplace_back(parentID, operation, std::move(object));
}

std::vector<Notifier> Notifier::Take() {
    QMutexLocker locker(&notifierMutex());
    auto copy = notifierPool();
    notifierPool().clear();
    return copy;
}

void NotifierMessage::addNotifier(const Notifier& notifier) {
    m_notifiers.push_back(notifier);
}

bool QualityControl::add(std::shared_ptr<WaveformQuality> quality) {
    if (!quality) {
        return false;
    }
    quality->setParent(this);
    m_waveformQualities.push_back(std::move(quality));
    return true;
}

bool DataAvailability::add(std::shared_ptr<DataExtent> extent) {
    if (!extent) {
        return false;
    }
    extent->setParent(this);
    m_extents.push_back(std::move(extent));
    return true;
}

bool Inventory::addStructure(std::shared_ptr<Structure> structure) {
    if (!structure) {
        return false;
    }
    structure->setParent(this);
    m_structures.push_back(std::move(structure));
    return true;
}

bool Inventory::addSensor(const QString& structureID, std::shared_ptr<Sensor> sensor) {
    if (!sensor) {
        return false;
    }

    auto* target = structure(structureID);
    if (!target) {
        return false;
    }

    sensor->setParent(target);
    target->sensors.push_back(std::move(sensor));
    return true;
}

Structure* Inventory::structure(const QString& publicID) const {
    for (const auto& item : m_structures) {
        if (item && item->publicID() == publicID) {
            return item.get();
        }
    }
    return nullptr;
}

bool EventParameters::add(std::shared_ptr<Event> event) {
    if (!event) {
        return false;
    }
    event->setParent(this);
    m_events.push_back(std::move(event));
    return true;
}

namespace strongmotion {

bool StrongMotionParameters::add(std::shared_ptr<Record> record) {
    if (!record) {
        return false;
    }
    record->setParent(this);
    m_records.push_back(std::move(record));
    return true;
}

} // namespace strongmotion
} // namespace tp::datamodel
