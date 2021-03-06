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

#include "digitallyimportedclient.h"
#include "digitallyimportedservicebase.h"
#include "digitallyimportedurlhandler.h"
#include "internetmodel.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/player.h"
#include "core/taskmanager.h"
#include "globalsearch/digitallyimportedsearchprovider.h"
#include "globalsearch/globalsearch.h"
#include "ui/iconloader.h"

#include <QDesktopServices>
#include <QMenu>
#include <QNetworkReply>
#include <QSettings>

const char* DigitallyImportedServiceBase::kSettingsGroup = "digitally_imported";
const int DigitallyImportedServiceBase::kStreamsCacheDurationSecs =
    60 * 60 * 24 * 14; // 2 weeks


DigitallyImportedServiceBase::DigitallyImportedServiceBase(
  const QString& name,
  const QString& description,
  const QUrl& homepage_url,
  const QIcon& icon,
  const QString& api_service_name,
  InternetModel* model, QObject* parent)
  : InternetService(name, model, parent),
    homepage_url_(homepage_url),
    icon_(icon),
    service_description_(description),
    api_service_name_(api_service_name),
    network_(new NetworkAccessManager(this)),
    url_handler_(new DigitallyImportedUrlHandler(this)),
    basic_audio_type_(1),
    premium_audio_type_(2),
    root_(NULL),
    context_item_(NULL),
    saved_channels_(kSettingsGroup, api_service_name, kStreamsCacheDurationSecs),
    api_client_(new DigitallyImportedClient(api_service_name, this))
{
  ReloadSettings();

  model->player()->RegisterUrlHandler(url_handler_);
  model->global_search()->AddProvider(new DigitallyImportedSearchProvider(this, this));

  basic_playlists_
      << "http://%1/public3/%2.pls"
      << "http://%1/public1/%2.pls"
      << "http://%1/public5/%2.asx";

  premium_playlists_
      << "http://%1/premium_high/%2.pls?hash=%3"
      << "http://%1/premium_medium/%2.pls?hash=%3"
      << "http://%1/premium/%2.pls?hash=%3"
      << "http://%1/premium_wma_low/%2.asx?hash=%3"
      << "http://%1/premium_wma/%2.asx?hash=%3";
}

DigitallyImportedServiceBase::~DigitallyImportedServiceBase() {
}

QStandardItem* DigitallyImportedServiceBase::CreateRootItem() {
  root_ = new QStandardItem(icon_, service_description_);
  root_->setData(true, InternetModel::Role_CanLazyLoad);
  return root_;
}

void DigitallyImportedServiceBase::LazyPopulate(QStandardItem* parent) {
  if (parent == root_) {
    RefreshStreams();
  }
}

void DigitallyImportedServiceBase::RefreshStreams() {
  if (IsChannelListStale()) {
    ForceRefreshStreams();
    return;
  }

  PopulateStreams();
}

void DigitallyImportedServiceBase::ForceRefreshStreams() {
  // Start a task to tell the user we're busy
  int task_id = model()->task_manager()->StartTask(tr("Getting streams"));

  QNetworkReply* reply = api_client_->GetChannelList();
  NewClosure(reply, SIGNAL(finished()),
             this, SLOT(RefreshStreamsFinished(QNetworkReply*,int)),
             reply, task_id);
}

void DigitallyImportedServiceBase::RefreshStreamsFinished(QNetworkReply* reply, int task_id) {
  model()->task_manager()->SetTaskFinished(task_id);
  reply->deleteLater();

  // Parse the list and sort by name
  DigitallyImportedClient::ChannelList channels = api_client_->ParseChannelList(reply);
  qSort(channels);

  saved_channels_.Update(channels);

  // Only update the item's children if it's already been populated
  if (!root_->data(InternetModel::Role_CanLazyLoad).toBool())
    PopulateStreams();

  emit StreamsChanged();
}

void DigitallyImportedServiceBase::PopulateStreams() {
  if (root_->hasChildren())
    root_->removeRows(0, root_->rowCount());

  // Add each stream to the model
  foreach (const DigitallyImportedClient::Channel& channel, saved_channels_) {
    Song song;
    SongFromChannel(channel, &song);

    QStandardItem* item = new QStandardItem(QIcon(":/last.fm/icon_radio.png"),
                                            song.title());
    item->setData(channel.description_, Qt::ToolTipRole);
    item->setData(InternetModel::PlayBehaviour_SingleItem, InternetModel::Role_PlayBehaviour);
    item->setData(QVariant::fromValue(song), InternetModel::Role_SongMetadata);
    root_->appendRow(item);
  }
}

void DigitallyImportedServiceBase::SongFromChannel(
    const DigitallyImportedClient::Channel& channel, Song* song) const {
  song->set_title(channel.name_);
  song->set_artist(service_description_ + " - " + channel.director_);
  song->set_url(QUrl(api_service_name_ + "://" + channel.key_));
  song->set_art_automatic(channel.art_url_.toString());
}

void DigitallyImportedServiceBase::Homepage() {
  QDesktopServices::openUrl(homepage_url_);
}

void DigitallyImportedServiceBase::ReloadSettings() {
  QSettings s;
  s.beginGroup(kSettingsGroup);

  basic_audio_type_ = s.value("basic_audio_type", 1).toInt();
  premium_audio_type_ = s.value("premium_audio_type", 2).toInt();
  username_ = s.value("username").toString();
  listen_hash_ = s.value("listen_hash").toString();
  saved_channels_.Load();
}

void DigitallyImportedServiceBase::ShowContextMenu(
    const QModelIndex& index, const QPoint& global_pos) {
  if (!context_menu_) {
    context_menu_.reset(new QMenu);
    context_menu_->addActions(GetPlaylistActions());
    context_menu_->addAction(IconLoader::Load("download"),
                             tr("Open %1 in browser").arg(homepage_url_.host()),
                             this, SLOT(Homepage()));
    context_menu_->addAction(IconLoader::Load("view-refresh"),
                             tr("Refresh streams"),
                             this, SLOT(ForceRefreshStreams()));
    context_menu_->addSeparator();
    context_menu_->addAction(IconLoader::Load("configure"),
                             tr("Configure..."),
                             this, SLOT(ShowSettingsDialog()));
  }

  context_item_ = model()->itemFromIndex(index);
  context_menu_->popup(global_pos);
}

bool DigitallyImportedServiceBase::is_premium_account() const {
  return !listen_hash_.isEmpty();
}

void DigitallyImportedServiceBase::LoadPlaylistFinished() {
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  if (!reply) {
    return;
  }
  reply->deleteLater();

  if (reply->header(QNetworkRequest::ContentTypeHeader).toString() == "text/html") {
    url_handler_->CancelTask();

    emit StreamError(tr("Error loading di.fm playlist"));
  } else {
    url_handler_->LoadPlaylistFinished(reply);
  }
}

void DigitallyImportedServiceBase::ShowSettingsDialog() {
  emit OpenSettingsAtPage(SettingsDialog::Page_DigitallyImported);
}

DigitallyImportedClient::ChannelList DigitallyImportedServiceBase::Channels() {
  if (IsChannelListStale()) {
    metaObject()->invokeMethod(this, "ForceRefreshStreams", Qt::QueuedConnection);
  }

  return saved_channels_;
}

void DigitallyImportedServiceBase::LoadStation(const QString& key) {
  QUrl playlist_url;

  // Replace "www." with "listen." in the hostname.
  const QString host = "listen." + homepage_url_.host().remove("www.");

  if (is_premium_account()) {
    playlist_url = QUrl(premium_playlists_[premium_audio_type_].arg(
                          host, key, listen_hash_));
  } else {
    playlist_url = QUrl(basic_playlists_[basic_audio_type_].arg(
                          host, key));
  }

  qLog(Debug) << "Getting playlist URL" << playlist_url;

  QNetworkReply* reply = network_->get(QNetworkRequest(playlist_url));
  connect(reply, SIGNAL(finished()), SLOT(LoadPlaylistFinished()));
}

QModelIndex DigitallyImportedServiceBase::GetCurrentIndex() {
  return context_item_->index();
}


DigitallyImportedService::DigitallyImportedService(InternetModel* model, QObject* parent)
  : DigitallyImportedServiceBase("DigitallyImported",
                                 "Digitally Imported",
                                 QUrl("http://www.di.fm"),
                                 QIcon(":/providers/digitallyimported.png"),
                                 "di",
                                 model, parent)
{
}

SkyFmService::SkyFmService(InternetModel* model, QObject* parent)
  : DigitallyImportedServiceBase("SKY.fm",
                                 "SKY.fm",
                                 QUrl("http://www.sky.fm"),
                                 QIcon(":/providers/skyfm.png"),
                                 "sky",
                                 model, parent)
{
}
