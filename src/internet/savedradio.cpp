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

#include "internetmodel.h"
#include "savedradio.h"
#include "core/mimedata.h"
#include "globalsearch/globalsearch.h"
#include "globalsearch/savedradiosearchprovider.h"
#include "ui/addstreamdialog.h"
#include "ui/iconloader.h"

#include <QSettings>
#include <QMenu>

const char* SavedRadio::kServiceName = "SavedRadio";
const char* SavedRadio::kSettingsGroup = "SavedRadio";

SavedRadio::SavedRadio(InternetModel* parent)
  : InternetService(kServiceName, parent, parent),
    context_menu_(NULL),
    root_(NULL)
{
  LoadStreams();

  model()->global_search()->AddProvider(new SavedRadioSearchProvider(this, this));
}

SavedRadio::~SavedRadio() {
  delete context_menu_;
}

QStandardItem* SavedRadio::CreateRootItem() {
  root_ = new QStandardItem(IconLoader::Load("document-open-remote"),
                            tr("Your radio streams"));
  root_->setData(true, InternetModel::Role_CanLazyLoad);
  return root_;
}

void SavedRadio::LazyPopulate(QStandardItem* item) {
  switch (item->data(InternetModel::Role_Type).toInt()) {
    case InternetModel::Type_Service:
      foreach (const Stream& stream, streams_)
        AddStreamToList(stream, root_);

      break;

    default:
      break;
  }
}

void SavedRadio::LoadStreams() {
  // Load saved streams
  QSettings s;
  s.beginGroup(kSettingsGroup);

  int count = s.beginReadArray("streams");
  for (int i=0 ; i<count ; ++i) {
    s.setArrayIndex(i);
    streams_ << Stream(s.value("url").toString(), s.value("name").toString());
  }
  s.endArray();
}

void SavedRadio::SaveStreams() {
  QSettings s;
  s.beginGroup(kSettingsGroup);

  int count = streams_.size();
  s.beginWriteArray("streams", count);
  for (int i=0 ; i<count ; ++i) {
    s.setArrayIndex(i);
    s.setValue("url", streams_[i].url_);
    s.setValue("name", streams_[i].name_);
  }
  s.endArray();

  emit StreamsChanged();
}

void SavedRadio::ShowContextMenu(const QModelIndex& index,
                                 const QPoint& global_pos) {
  if (!context_menu_) {
    context_menu_ = new QMenu;
    context_menu_->addActions(GetPlaylistActions());
    remove_action_ = context_menu_->addAction(IconLoader::Load("list-remove"), tr("Remove"), this, SLOT(Remove()));
    edit_action_ = context_menu_->addAction(IconLoader::Load("edit-rename"), tr("Edit..."), this, SLOT(Edit()));
    context_menu_->addSeparator();
    context_menu_->addAction(IconLoader::Load("document-open-remote"), tr("Add another stream..."), this, SIGNAL(ShowAddStreamDialog()));
  }

  context_item_ = model()->itemFromIndex(index);
  const bool is_root = index.data(InternetModel::Role_Type).toInt() == InternetModel::Type_Service;

  GetAppendToPlaylistAction()->setEnabled(!is_root);
  GetReplacePlaylistAction()->setEnabled(!is_root);
  GetOpenInNewPlaylistAction()->setEnabled(!is_root);
  remove_action_->setEnabled(!is_root);
  edit_action_->setEnabled(!is_root);

  context_menu_->popup(global_pos);
}

void SavedRadio::Remove() {
  streams_.removeAll(Stream(QUrl(context_item_->data(InternetModel::Role_Url).toUrl())));
  context_item_->parent()->removeRow(context_item_->row());
  SaveStreams();
}

void SavedRadio::Edit() {
  if (!edit_dialog_) {
    edit_dialog_.reset(new AddStreamDialog);
    edit_dialog_->set_save_visible(false);
  }

  edit_dialog_->set_name(context_item_->text());
  edit_dialog_->set_url(context_item_->data(InternetModel::Role_Url).toUrl());
  if (edit_dialog_->exec() == QDialog::Rejected)
    return;

  int i = streams_.indexOf(Stream(QUrl(context_item_->data(InternetModel::Role_Url).toUrl())));
  Stream& stream = streams_[i];
  stream.name_ = edit_dialog_->name();
  stream.url_ = edit_dialog_->url();

  context_item_->setText(stream.name_);
  context_item_->setData(stream.url_, InternetModel::Role_Url);

  SaveStreams();
}

QModelIndex SavedRadio::GetCurrentIndex() {
  return context_item_->index();
}

void SavedRadio::AddStreamToList(const Stream& stream, QStandardItem* parent) {
  QStandardItem* s = new QStandardItem(QIcon(":last.fm/icon_radio.png"), stream.name_);
  s->setData(stream.url_, InternetModel::Role_Url);
  s->setData(InternetModel::PlayBehaviour_UseSongLoader, InternetModel::Role_PlayBehaviour);
  parent->appendRow(s);
}

void SavedRadio::Add(const QUrl &url, const QString& name) {
  if (streams_.contains(Stream(url)))
    return;

  Stream stream(url, name);
  streams_ << stream;

  if (!root_->data(InternetModel::Role_CanLazyLoad).toBool()) {
    AddStreamToList(stream, root_);
  }
  SaveStreams();
}
