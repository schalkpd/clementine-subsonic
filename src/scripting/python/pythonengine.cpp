/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>
#include <frameobject.h>
#include <sip.h>

#include "pythonengine.h"
#include "pythonscript.h"
#include "sipAPIclementine.h"
#include "covers/coverproviders.h"
#include "library/library.h"

#include <QFile>
#include <QtDebug>

const char* PythonEngine::kModulePrefix = "clementinescripts";
PythonEngine* PythonEngine::sInstance = NULL;

extern "C" {
  void initclementine();

#ifdef Q_OS_WIN32
  void initsip();
  void initQt();
  void initQtCore();
  void initQtGui();
  void initQtNetwork();
#endif
}

PythonEngine::PythonEngine(ScriptManager* manager)
  : LanguageEngine(manager),
    initialised_(false)
{
  Q_ASSERT(sInstance == NULL);
  sInstance = this;
  #ifdef Q_OS_DARWIN
    setenv("PYTHONPATH", (QCoreApplication::applicationDirPath() + "/../PlugIns").toLocal8Bit().constData(), 1);
  #endif
}

PythonEngine::~PythonEngine() {
  sInstance = NULL;

  if (initialised_) {
    Py_Finalize();
  }
}

const sipAPIDef* PythonEngine::GetSIPApi() {
#if defined(SIP_USE_PYCAPSULE)
  return (const sipAPIDef *)PyCapsule_Import("sip._C_API", 0);
#else
  PyObject *sip_module;
  PyObject *sip_module_dict;
  PyObject *c_api;

  /* Import the SIP module. */
  sip_module = PyImport_ImportModule("sip");

  if (sip_module == NULL)
      return NULL;

  /* Get the module's dictionary. */
  sip_module_dict = PyModule_GetDict(sip_module);

  /* Get the "_C_API" attribute. */
  c_api = PyDict_GetItemString(sip_module_dict, "_C_API");

  if (c_api == NULL)
      return NULL;

  /* Sanity check that it is the right type. */
  if (!PyCObject_Check(c_api))
      return NULL;

  /* Get the actual pointer from the object. */
  return (const sipAPIDef *)PyCObject_AsVoidPtr(c_api);
#endif
}

bool PythonEngine::EnsureInitialised() {
  if (initialised_)
    return true;

  AddLogLine("Initialising python...", false);

#ifdef Q_OS_WIN32
  // On Windows we statically link against SIP and PyQt, so add those modules
  // to Python's inittab here.
  PyImport_AppendInittab(const_cast<char*>("sip"), initsip);
  PyImport_AppendInittab(const_cast<char*>("PyQt4.Qt"), initQt);
  PyImport_AppendInittab(const_cast<char*>("PyQt4.QtCore"), initQtCore);
  PyImport_AppendInittab(const_cast<char*>("PyQt4.QtGui"), initQtGui);
  PyImport_AppendInittab(const_cast<char*>("PyQt4.QtNetwork"), initQtNetwork);
#endif

  // Add the Clementine builtin module
  PyImport_AppendInittab(const_cast<char*>("clementine"), initclementine);

  // Initialise python
  Py_SetProgramName(const_cast<char*>("clementine"));
  PyEval_InitThreads();
  Py_InitializeEx(0);

  // Get the clementine module so we can put stuff in it
  clementine_module_ = PyImport_ImportModule("clementine");
  if (!clementine_module_) {
    AddLogLine("Failed to import the clementine module", true);
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    Py_Finalize();
    return false;
  }
  sip_api_ = GetSIPApi();

  // Add objects to the module
  if (manager()->data().valid_) {
    AddObject(manager()->data().library_->backend(), sipType_LibraryBackend, "library");
    AddObject(manager()->data().library_view_, sipType_LibraryView, "library_view");
    AddObject(manager()->data().player_, sipType_PlayerInterface, "player");
    AddObject(manager()->data().playlists_, sipType_PlaylistManagerInterface, "playlists");
    AddObject(manager()->data().radio_model_, sipType_RadioModel, "radio_model");
    AddObject(manager()->data().settings_dialog_, sipType_SettingsDialog, "settings_dialog");
    AddObject(manager()->data().task_manager_, sipType_TaskManager, "task_manager");
    AddObject(&CoverProviders::instance(), sipType_CoverProviders, "cover_providers");
  }

  AddObject(manager()->ui(), sipType_UIInterface, "ui");
  AddObject(this, sipType_PythonEngine, "pythonengine");

  // Create a module for scripts
  PyImport_AddModule(kModulePrefix);

  // Run the startup script - this redirects sys.stdout and sys.stderr to our
  // log handler.
  QFile python_startup(":pythonstartup.py");
  python_startup.open(QIODevice::ReadOnly);
  QByteArray python_startup_script = python_startup.readAll();

  if (PyRun_SimpleString(python_startup_script.constData()) != 0) {
    AddLogLine("Could not execute startup code", true);
    Py_Finalize();
    return false;
  }

  PyEval_ReleaseLock();

  initialised_ = true;
  return true;
}

Script* PythonEngine::CreateScript(const ScriptInfo& info) {
  // Initialise Python if it hasn't been done yet
  if (!EnsureInitialised()) {
    return NULL;
  }

  Script* ret = new PythonScript(this, info);
  loaded_scripts_[ret->info().id()] = ret; // Used by RegisterNativeObject during startup
  if (ret->Init()) {
    return ret;
  }

  DestroyScript(ret);
  return NULL;
}

void PythonEngine::DestroyScript(Script* script) {
  script->Unload();
  loaded_scripts_.remove(script->info().id());
  delete script;
}

void PythonEngine::AddObject(void* object, const _sipTypeDef* type,
                             const char * name) const {
  PyObject* python_object = sip_api_->api_convert_from_type(object, type, NULL);
  PyModule_AddObject(clementine_module_, name, python_object);
}

void PythonEngine::AddLogLine(const QString& message, bool error) {
  manager()->AddLogLine("Python", message, error);
}

Script* PythonEngine::FindScriptMatchingId(const QString& id) const {
  foreach (const QString& script_id, loaded_scripts_.keys()) {
    if (script_id == id || id.startsWith(script_id + ".")) {
      return loaded_scripts_[script_id];
    }
  }
  return NULL;
}

void PythonEngine::RegisterNativeObject(QObject* object) {
  // This function is called from Python, we need to figure out which script
  // called it, so we look at the __package__ variable in the bottom stack
  // frame.

  PyFrameObject* frame = PyEval_GetFrame();
  if (!frame) {
    qWarning() << __PRETTY_FUNCTION__ << "unable to get stack frame";
    return;
  }
  while (frame->f_back) {
    frame = frame->f_back;
  }

  PyObject* __package__ = PyMapping_GetItemString(
      frame->f_globals, const_cast<char*>("__package__"));
  if (!__package__) {
    qWarning() << __PRETTY_FUNCTION__ << "unable to get __package__";
    return;
  }

  QString package = PyString_AsString(__package__);
  Py_DECREF(__package__);
  package.remove(QString(kModulePrefix) + ".");

  Script* script = FindScriptMatchingId(package);
  if (!script) {
    qWarning() << __PRETTY_FUNCTION__ << "unable to find script for package" << package;
    return;
  }

  // Finally got the script - tell it about this object so it will get destroyed
  // when the script is unloaded.
  script->AddNativeObject(object);

  // Save the script as a property on the object so we can remove it later
  object->setProperty("owning_python_script", QVariant::fromValue(script));
  connect(object, SIGNAL(destroyed(QObject*)), SLOT(NativeObjectDestroyed(QObject*)));
}

void PythonEngine::NativeObjectDestroyed(QObject* object) {
  Script* script = object->property("owning_python_script").value<Script*>();
  if (!script || !loaded_scripts_.values().contains(script))
    return;

  script->RemoveNativeObject(object);
}
