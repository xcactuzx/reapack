/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2017  Christian Fillion
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "browser_entry.hpp"

#include "config.hpp"
#include "index.hpp"
#include "menu.hpp"
#include "reapack.hpp"

#include <boost/range/adaptor/reversed.hpp>

using namespace std;

Browser::Entry::Entry(const Package *pkg, const Registry::Entry &re, const IndexPtr &i)
  : m_flags(0), regEntry(re), package(pkg), index(i), current(nullptr)
{
  const auto &instOpts = g_reapack->config()->install;
  latest = pkg->lastVersion(instOpts.bleedingEdge, regEntry.version);

  if(regEntry) {
    m_flags |= InstalledFlag;

    if(latest && regEntry.version < latest->name())
      m_flags |= OutOfDateFlag;

    current = pkg->findVersion(regEntry.version);
  }
  else
    m_flags |= UninstalledFlag;

  // Show latest pre-release if no stable version is available,
  // or the newest available version if older than current installed version.
  if(!latest)
    latest = pkg->lastVersion(true);
}

Browser::Entry::Entry(const Registry::Entry &re, const IndexPtr &i)
  : m_flags(InstalledFlag | ObsoleteFlag), regEntry(re), package(nullptr),
  index(i), current(nullptr), latest(nullptr)
{}

string Browser::Entry::displayState() const
{
  string state;

  if(test(ObsoleteFlag))
    state += 'o';
  else if(test(OutOfDateFlag))
    state += 'u';
  else if(test(InstalledFlag))
    state += 'i';
  else
    state += '\x20';

  if(regEntry.pinned)
    state += 'p';

  if(target)
    state += *target == nullptr ? 'R' : 'I';
  if(pin && canPin())
    state += 'P';

  return state;
}

const string &Browser::Entry::indexName() const
{
  return package ? package->category()->index()->name() : regEntry.remote;
}

const string &Browser::Entry::categoryName() const
{
  return package ? package->category()->name() : regEntry.category;
}

const string &Browser::Entry::packageName() const
{
  return package ? package->name() : regEntry.package;
}

string Browser::Entry::displayName() const
{
  if(package)
    return package->displayName();
  else
    return Package::displayName(regEntry.package, regEntry.description);
}

Package::Type Browser::Entry::type() const
{
  return latest ? package->type() : regEntry.type;
}

string Browser::Entry::displayType() const
{
  return package ? package->displayType() : Package::displayType(regEntry.type);
}

string Browser::Entry::displayVersion() const
{
  string display;

  if(test(InstalledFlag))
    display = regEntry.version.toString();

  if(latest && (!regEntry || latest->name() > regEntry.version)) {
    if(!display.empty())
      display += '\x20';

    display += '(' + latest->name().toString() + ')';
  }

  return display;
}

const VersionName *Browser::Entry::sortVersion() const
{
  if(test(InstalledFlag))
    return &regEntry.version;
  else
    return &latest->name();
}

string Browser::Entry::displayAuthor() const
{
  return latest ? latest->displayAuthor() : Version::displayAuthor(regEntry.author);
}

const Time *Browser::Entry::lastUpdate() const
{
  return latest ? &latest->time() : nullptr;
}

Remote Browser::Entry::remote() const
{
  return g_reapack->remote(indexName());
}

ListView::Row Browser::Entry::makeRow() const
{
  const Time *time = lastUpdate();

  ListView::Row row;
  row.userData = (void *)this;
  row.push_back(make_autostring(displayState()));
  row.push_back(make_autostring(displayName()));
  row.push_back(make_autostring(categoryName()));
  row.push_back({make_autostring(displayVersion()), (void *)sortVersion()});
  row.push_back(make_autostring(displayAuthor()));
  row.push_back(make_autostring(displayType()));
  row.push_back(make_autostring(indexName()));
  row.push_back({time ? make_autostring(time->toString()) : auto_string(),
    (void *)time});

  return row;
}

void Browser::Entry::fillMenu(Menu &menu) const
{
  if(test(InstalledFlag)) {
    if(test(OutOfDateFlag)) {
      auto_char installLabel[32];
      auto_snprintf(installLabel, auto_size(installLabel),
        AUTO_STR("U&pdate to v%s"),
        make_autostring(latest->name().toString()).c_str());

      const UINT actionIndex = menu.addAction(installLabel, ACTION_LATEST);
      if(target && *target == latest)
        menu.check(actionIndex);
    }

    auto_char reinstallLabel[32];
    auto_snprintf(reinstallLabel, auto_size(reinstallLabel),
      AUTO_STR("&Reinstall v%s"),
      make_autostring(regEntry.version.toString()).c_str());

    const UINT actionIndex = menu.addAction(reinstallLabel, ACTION_REINSTALL);
    if(!current || test(ObsoleteFlag))
      menu.disable(actionIndex);
    else if(target && *target == current)
      menu.check(actionIndex);
  }
  else {
    auto_char installLabel[32];
    auto_snprintf(installLabel, auto_size(installLabel),
      AUTO_STR("&Install v%s"),
      make_autostring(latest->name().toString()).c_str());

    const UINT actionIndex = menu.addAction(installLabel, ACTION_LATEST);
    if(target && *target == latest)
      menu.check(actionIndex);
  }

  Menu versionMenu = menu.addMenu(AUTO_STR("Versions"));
  const UINT versionMenuIndex = menu.size() - 1;
  if(test(ObsoleteFlag))
    menu.disable(versionMenuIndex);
  else {
    const auto &versions = package->versions();
    int verIndex = (int)versions.size();
    for(const Version *ver : versions | boost::adaptors::reversed) {
      const UINT actionIndex = versionMenu.addAction(
        make_autostring(ver->name().toString()).c_str(),
        --verIndex | (ACTION_VERSION << 8));

      if(target ? *target == ver : ver == current) {
        if(target && ver != latest)
          menu.check(versionMenuIndex);

        versionMenu.checkRadio(actionIndex);
      }
    }
  }

  const UINT pinIndex = menu.addAction(
    AUTO_STR("&Pin current version"), ACTION_PIN);
  if(!canPin())
    menu.disable(pinIndex);
  if(pin.value_or(regEntry.pinned))
    menu.check(pinIndex);

  const UINT uninstallIndex =
    menu.addAction(AUTO_STR("&Uninstall"), ACTION_UNINSTALL);
  if(!test(InstalledFlag) || remote().isProtected())
    menu.disable(uninstallIndex);
  else if(target && *target == nullptr)
    menu.check(uninstallIndex);

  menu.addSeparator();

  menu.setEnabled(!test(ObsoleteFlag),
    menu.addAction(AUTO_STR("About this &package"), ACTION_ABOUT_PKG));

  auto_char aboutLabel[64];
  const auto_string &name = make_autostring(indexName());
  auto_snprintf(aboutLabel, auto_size(aboutLabel),
    AUTO_STR("&About %s..."), name.c_str());
  menu.addAction(aboutLabel, ACTION_ABOUT_REMOTE);
}

bool Browser::Entry::operator==(const Entry &o) const
{
  return indexName() == o.indexName() && categoryName() == o.categoryName() &&
    packageName() == o.packageName();
}
