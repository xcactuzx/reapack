#include "package.hpp"

#include "database.hpp"
#include "errors.hpp"

Package::Type Package::convertType(const char *type)
{
  if(!strcmp(type, "script"))
    return ScriptType;
  else
    return UnknownType;
}

Package::Package(const Type type, const std::string &name)
  : m_category(0), m_type(type), m_name(name)
{
  if(m_name.empty())
    throw reapack_error("empty package name");
}

void Package::addVersion(VersionPtr ver)
{
  if(ver->sources().empty())
    return;

  m_versions.insert(ver);
}

VersionPtr Package::version(const int index) const
{
  auto it = m_versions.begin();

  for(int i = 0; i < index; i++)
    it++;

  return *it;
}

VersionPtr Package::lastVersion() const
{
  return *prev(m_versions.end());
}

Path Package::targetLocation() const
{
  switch(m_type) {
  case ScriptType:
    return scriptLocation();
  default:
    throw reapack_error("unsupported package type");
  }
}

Path Package::scriptLocation() const
{
  // TODO: use actual database name instead of hard-coded "ReaScripts"
  Path path;
  path.append("Scripts");
  path.append("ReaScripts");

  if(m_category)
    path.append(category()->name());

  path.append(m_name);

  return path;
}
