#include "osl/file.hxx"
#include "rtl/ref.hxx"
#include <rtl/string.hxx>
#include <rtl/ustring.hxx>
#include <unoidl/unoidl.hxx>
#include <map>
#include <set>

namespace writer {
struct Entity {
    enum class Sorted { NO, ACTIVE, YES };
    enum class Written { NO, DECLARATION, DEFINITION };

    explicit Entity(rtl::Reference<unoidl::Entity> const& theEntity, bool theRelevant)
        : entity(theEntity)
        , relevant(theRelevant)
        , sorted(Sorted::NO)
        , written(Written::NO) {}

    rtl::Reference<unoidl::Entity> const entity;
    std::set<OUString> dependencies;
    std::set<OUString> interfaceDependencies;
    std::map<OUString, Entity*> module;
    bool relevant;
    Sorted sorted;
    Written written;
};

/**
 * Takes a string <type>: produces the <rank> (eg. 0 = float, 1 = sequence<float>, etc.),
 * a vector of <typeArguments> for polymorphic structs, and whether the type is an IDL-defined
 * <entity> (interface, struct, service, basically any non-primitive)
 */
OUString decomposeType(OUString const& type, std::size_t* rank,
                       std::vector<OUString>* typeArguments, bool* entity);

void mapEntities(rtl::Reference<unoidl::Manager> const& manager, OUString const& uri,
                 std::map<OUString, writer::Entity*>& map,
                 std::map<OUString, writer::Entity*>& flatMap);

OUString entityName(OUString const& name);
OUString entityNamespace(OUString const& name);
OUString simplifyNamespace(OUString const& name);

class BaseWriter {
public:
    BaseWriter(std::map<OUString, Entity*> entities, OUString const& outDirectoryUrl)
        : entities_(entities)
        , outDirectoryUrl_(outDirectoryUrl){};
    virtual void writeEntity(OUString const& name);
    virtual ~BaseWriter() = default;
    void createEntityFile(OUString const& entityName, OUString const& suffix);
    void close();

protected:
    std::map<OUString, Entity*> entities_;
    OUString outDirectoryUrl_;
    OUString currentEntity_;
    void out(OUString const& text);

    virtual void writeDoc(rtl::Reference<unoidl::Entity> const& entity);
    virtual void writeDoc(OUString const& doc);

    virtual void writeName(OUString const& name) = 0;
    virtual OUString translateSimpleType(OUString const& name) = 0;
    virtual void writeType(OUString const& name) = 0;
    /** Declares interface dependencies/imports/includes */
    virtual void writeInterfaceDependency(OUString const& dependentName,
                                          OUString const& dependencyName, bool published)
        = 0;
    virtual void writeEnum(OUString const& name, rtl::Reference<unoidl::EnumTypeEntity> entity) = 0;
    virtual void writePlainStruct(OUString const& name,
                                  rtl::Reference<unoidl::PlainStructTypeEntity> entity)
        = 0;
    virtual void
    writePolymorphicStruct(OUString const& name,
                           rtl::Reference<unoidl::PolymorphicStructTypeTemplateEntity> entity)
        = 0;
    virtual void writeException(OUString const& name,
                                rtl::Reference<unoidl::ExceptionTypeEntity> entity)
        = 0;
    virtual void writeInterface(OUString const& name,
                                rtl::Reference<unoidl::InterfaceTypeEntity> entity)
        = 0;
    virtual void writeTypedef(OUString const& name, rtl::Reference<unoidl::TypedefEntity> entity)
        = 0;
    virtual void writeConstantGroup(OUString const& name,
                                    rtl::Reference<unoidl::ConstantGroupEntity> entity)
        = 0;
    virtual void
    writeSingleInterfaceService(OUString const& name,
                                rtl::Reference<unoidl::SingleInterfaceBasedServiceEntity> entity)
        = 0;
    virtual void
    writeAccumulationService(OUString const& name,
                             rtl::Reference<unoidl::AccumulationBasedServiceEntity> entity)
        = 0;
    virtual void
    writeInterfaceSingleton(OUString const& name,
                            rtl::Reference<unoidl::InterfaceBasedSingletonEntity> entity)
        = 0;
    virtual void writeServiceSingleton(OUString const& name,
                                       rtl::Reference<unoidl::ServiceBasedSingletonEntity> entity)
        = 0;

private:
    osl::File* file_;
};

class V8Writer : public BaseWriter {
public:
    V8Writer(std::map<OUString, Entity*> entities, OUString const& outDirectoryUrl)
        : BaseWriter(entities, outDirectoryUrl) {}

private:
    void writeName(OUString const& name);
    OUString translateSimpleType(OUString const& name);
    void writeType(OUString const& name);
    void writeInterfaceDependency(OUString const& dependentName, OUString const& dependencyName,
                                  bool published);
    void writeEnum(OUString const& name, rtl::Reference<unoidl::EnumTypeEntity> entity);
    void writePlainStruct(OUString const& name,
                          rtl::Reference<unoidl::PlainStructTypeEntity> entity);
    void writePolymorphicStruct(OUString const& name,
                                rtl::Reference<unoidl::PolymorphicStructTypeTemplateEntity> entity);
    void writeException(OUString const& name, rtl::Reference<unoidl::ExceptionTypeEntity> entity);
    void writeInterface(OUString const& name, rtl::Reference<unoidl::InterfaceTypeEntity> entity);
    void writeTypedef(OUString const& name, rtl::Reference<unoidl::TypedefEntity> entity);
    void writeConstantGroup(OUString const& name,
                            rtl::Reference<unoidl::ConstantGroupEntity> entity);
    void
    writeSingleInterfaceService(OUString const& name,
                                rtl::Reference<unoidl::SingleInterfaceBasedServiceEntity> entity);
    void writeAccumulationService(OUString const& name,
                                  rtl::Reference<unoidl::AccumulationBasedServiceEntity> entity);
    void writeInterfaceSingleton(OUString const& name,
                                 rtl::Reference<unoidl::InterfaceBasedSingletonEntity> entity);
    void writeServiceSingleton(OUString const& name,
                               rtl::Reference<unoidl::ServiceBasedSingletonEntity> entity);
};

class TypeScriptWriter : public BaseWriter {
public:
    TypeScriptWriter(std::map<OUString, Entity*> entities, OUString const& outDirectoryUrl)
        : BaseWriter(entities, outDirectoryUrl) {}
    void writeTSIndex(OUString const& name, Entity* moduleEntity);

private:
    void writeName(OUString const& name);
    OUString translateSimpleType(OUString const& name);
    void writeType(OUString const& name);
    void writeInterfaceDependency(OUString const& dependentName, OUString const& dependencyName,
                                  bool published);
    void writeEnum(OUString const& name, rtl::Reference<unoidl::EnumTypeEntity> entity);
    void writePlainStruct(OUString const& name,
                          rtl::Reference<unoidl::PlainStructTypeEntity> entity);
    void writePolymorphicStruct(OUString const& name,
                                rtl::Reference<unoidl::PolymorphicStructTypeTemplateEntity> entity);
    void writeException(OUString const& name, rtl::Reference<unoidl::ExceptionTypeEntity> entity);
    void writeInterface(OUString const& name, rtl::Reference<unoidl::InterfaceTypeEntity> entity);
    void writeTypedef(OUString const& name, rtl::Reference<unoidl::TypedefEntity> entity);
    void writeConstantGroup(OUString const& name,
                            rtl::Reference<unoidl::ConstantGroupEntity> entity);
    void
    writeSingleInterfaceService(OUString const& name,
                                rtl::Reference<unoidl::SingleInterfaceBasedServiceEntity> entity);
    void writeAccumulationService(OUString const& name,
                                  rtl::Reference<unoidl::AccumulationBasedServiceEntity> entity);
    void writeInterfaceSingleton(OUString const& name,
                                 rtl::Reference<unoidl::InterfaceBasedSingletonEntity> entity);
    void writeServiceSingleton(OUString const& name,
                               rtl::Reference<unoidl::ServiceBasedSingletonEntity> entity);

    std::map<OUString, int> dependentNamespace{};
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */