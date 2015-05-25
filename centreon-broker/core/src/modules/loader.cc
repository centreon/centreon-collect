/*
** Copyright 2011-2013,2015 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <QDir>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/modules/loader.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::modules;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
loader::loader() {}

/**
 *  Copy constructor.
 *
 *  @param[in] l Object to copy.
 */
loader::loader(loader const& l) : _handles(l._handles) {}

/**
 *  Destructor.
 */
loader::~loader() {
  unload();
}

/**
 *  Assignment operator.
 *
 *  @param[in] l Object to copy.
 *
 *  @return This object.
 */
loader& loader::operator=(loader const& l) {
  _handles = l._handles;
  return (*this);
}

/**
 *  Get iterator to the first module.
 *
 *  @return Iterator to the first module.
 */
loader::iterator loader::begin() {
  return (_handles.begin());
}

/**
 *  Get last iterator of the module list.
 *
 *  @return Last iterator of the module list.
 */
loader::iterator loader::end() {
  return (_handles.end());
}

/**
 *  Load a directory containing plugins.
 *
 *  @param[in] dirname Directory name.
 *  @param[in] arg     Module argument.
 */
void loader::load_dir(std::string const& dirname, void const* arg) {
  // Debug message.
  logging::debug(logging::medium)
    << "modules: loading directory '" << dirname << "'";

  // Set directory browsing parameters.
  QDir dir(dirname.c_str());
  QStringList l;
#ifdef Q_OS_WIN32
  l.push_back("*.dll");
#else
  l.push_back("*.so");
#endif /* Q_OS_WIN32 */
  dir.setNameFilters(l);

  // Iterate through all modules in directory.
  l = dir.entryList();
  for (QStringList::iterator it(l.begin()), end(l.end());
       it != end;
       ++it) {
    std::string file(dirname);
    file.append("/");
    file.append(it->toStdString());
    try {
      load_file(file, arg);
    }
    catch (exceptions::msg const& e) {
      logging::error(logging::high) << e.what();
    }
  }

  // Ending log message.
  logging::debug(logging::medium)
    << "modules: finished loading directory '" << dirname << "'";

  return ;
}

/**
 *  Load a plugin.
 *
 *  @param[in] filename File name.
 *  @param[in] arg      Module argument.
 */
void loader::load_file(std::string const& filename, void const* arg) {
  umap<std::string, misc::shared_ptr<handle> >::iterator
    it(_handles.find(filename));
  if (it == _handles.end()) {
    misc::shared_ptr<handle> handl(new handle);
    handl->open(filename, arg);
    _handles[filename] = handl;
  }
  else {
    logging::info(logging::low) << "modules: attempt to load '"
      << filename << "' which is already loaded";
    it->second->update(arg);
  }
  return ;
}

/**
 *  Unload modules.
 */
void loader::unload() {
  std::string key;
  while (!_handles.empty()) {
    umap<std::string, misc::shared_ptr<handle> >::iterator
      it(_handles.begin());
    key = it->first;
    umap<std::string, misc::shared_ptr<handle> >::iterator
      end(_handles.end());
    while (++it != end)
      if (it->first > key)
        key = it->first;
    _handles.erase(key);
  }
  return ;
}
