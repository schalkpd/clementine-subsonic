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

#ifndef PLAYLISTMANAGER_H
#define PLAYLISTMANAGER_H

#include <QColor>
#include <QItemSelection>
#include <QMap>
#include <QObject>

#include "core/song.h"
#include "smartplaylists/generator_fwd.h"

class LibraryBackend;
class Playlist;
class PlaylistBackend;
class PlaylistContainer;
class PlaylistParser;
class PlaylistSequence;
class SpecialPlaylistType;
class TaskManager;

class QModelIndex;
class QUrl;

class PlaylistManagerInterface : public QObject {
  Q_OBJECT

public:
  PlaylistManagerInterface(QObject* parent)
    : QObject(parent) {}

  virtual int current_id() const = 0;
  virtual int active_id() const = 0;

  virtual Playlist* playlist(int id) const = 0;
  virtual Playlist* current() const = 0;
  virtual Playlist* active() const = 0;

  // Returns the collection of playlists managed by this PlaylistManager.
  virtual QList<Playlist*> GetAllPlaylists() const = 0;
  // Grays out and reloads all deleted songs in all playlists.
  virtual void InvalidateDeletedSongs() = 0;
  // Removes all deleted songs from all playlists.
  virtual void RemoveDeletedSongs() = 0;

  virtual QItemSelection selection(int id) const = 0;
  virtual QItemSelection current_selection() const = 0;
  virtual QItemSelection active_selection() const = 0;

  virtual QString GetPlaylistName(int index) const = 0;

  virtual TaskManager* task_manager() const = 0;
  virtual LibraryBackend* library_backend() const = 0;
  virtual PlaylistBackend* playlist_backend() const = 0;
  virtual PlaylistSequence* sequence() const = 0;
  virtual PlaylistParser* parser() const = 0;
  virtual PlaylistContainer* playlist_container() const = 0;

  virtual void RegisterSpecialPlaylistType(SpecialPlaylistType* type) = 0;
  virtual void UnregisterSpecialPlaylistType(SpecialPlaylistType* type) = 0;
  virtual SpecialPlaylistType* GetPlaylistType(const QString& type) const = 0;

public slots:
  virtual void New(const QString& name, const SongList& songs = SongList(),
                   const QString& special_type = QString()) = 0;
  virtual void Load(const QString& filename) = 0;
  virtual void Save(int id, const QString& filename) = 0;
  virtual void Rename(int id, const QString& new_name) = 0;
  virtual void Remove(int id) = 0;
  virtual void ChangePlaylistOrder(const QList<int>& ids) = 0;

  virtual void SongChangeRequestProcessed(const QUrl& url, bool valid) = 0;

  virtual void SetCurrentPlaylist(int id) = 0;
  virtual void SetActivePlaylist(int id) = 0;
  virtual void SetActiveToCurrent() = 0;

  virtual void SelectionChanged(const QItemSelection& selection) = 0;

  // Convenience slots that defer to either current() or active()
  virtual void ClearCurrent() = 0;
  virtual void ShuffleCurrent() = 0;
  virtual void SetActivePlaying() = 0;
  virtual void SetActivePaused() = 0;
  virtual void SetActiveStopped() = 0;
  virtual void SetActiveStreamMetadata(const QUrl& url, const Song& song) = 0;
  // Rate current song using 0.0 - 1.0 scale.
  virtual void RateCurrentSong(double rating) = 0;
  // Rate current song using 0 - 5 scale.
  virtual void RateCurrentSong(int rating) = 0;

  virtual void PlaySmartPlaylist(smart_playlists::GeneratorPtr generator, bool as_new, bool clear) = 0;

signals:
  void PlaylistManagerInitialized();

  void PlaylistAdded(int id, const QString& name);
  void PlaylistRemoved(int id);
  void PlaylistRenamed(int id, const QString& new_name);
  void CurrentChanged(Playlist* new_playlist);
  void ActiveChanged(Playlist* new_playlist);

  void Error(const QString& message);
  void SummaryTextChanged(const QString& summary);

  // Forwarded from individual playlists
  void CurrentSongChanged(const Song& song);

  // Signals that one of manager's playlists has changed (new items, new
  // ordering etc.) - the argument shows which.
  void PlaylistChanged(Playlist* playlist);
  void EditingFinished(const QModelIndex& index);
  void PlayRequested(const QModelIndex& index);
};

class PlaylistManager : public PlaylistManagerInterface {
  Q_OBJECT

public:
  PlaylistManager(TaskManager* task_manager, QObject *parent = 0);
  ~PlaylistManager();

  int current_id() const { return current_; }
  int active_id() const { return active_; }

  Playlist* playlist(int id) const { return playlists_[id].p; }
  Playlist* current() const { return playlist(current_id()); }
  Playlist* active() const { return playlist(active_id()); }

  // Returns the collection of playlists managed by this PlaylistManager.
  QList<Playlist*> GetAllPlaylists() const;
  // Grays out and reloads all deleted songs in all playlists.
  void InvalidateDeletedSongs();
  // Removes all deleted songs from all playlists.
  void RemoveDeletedSongs();

  // Returns a pretty automatic name for playlist created from the given list of
  // songs.
  static QString GetNameForNewPlaylist(const SongList& songs);

  QItemSelection selection(int id) const;
  QItemSelection current_selection() const { return selection(current_id()); }
  QItemSelection active_selection() const { return selection(active_id()); }

  QString GetPlaylistName(int index) const { return playlists_[index].name; }

  void Init(LibraryBackend* library_backend, PlaylistBackend* playlist_backend,
            PlaylistSequence* sequence, PlaylistContainer* playlist_container);

  TaskManager* task_manager() const { return task_manager_; }
  LibraryBackend* library_backend() const { return library_backend_; }
  PlaylistBackend* playlist_backend() const { return playlist_backend_; }
  PlaylistSequence* sequence() const { return sequence_; }
  PlaylistParser* parser() const { return parser_; }
  PlaylistContainer* playlist_container() const { return playlist_container_; }

  void RegisterSpecialPlaylistType(SpecialPlaylistType* type);
  void UnregisterSpecialPlaylistType(SpecialPlaylistType* type);
  SpecialPlaylistType* GetPlaylistType(const QString& type) const;

public slots:
  void New(const QString& name, const SongList& songs = SongList(),
           const QString& special_type = QString());
  void Load(const QString& filename);
  void Save(int id, const QString& filename);
  void Rename(int id, const QString& new_name);
  void Remove(int id);
  void ChangePlaylistOrder(const QList<int>& ids);

  void SetCurrentPlaylist(int id);
  void SetActivePlaylist(int id);
  void SetActiveToCurrent() { SetActivePlaylist(current_id()); }

  void SelectionChanged(const QItemSelection& selection);

  // Convenience slots that defer to either current() or active()
  void ClearCurrent();
  void ShuffleCurrent();
  void SetActivePlaying();
  void SetActivePaused();
  void SetActiveStopped();
  void SetActiveStreamMetadata(const QUrl& url, const Song& song);
  // Rate current song using 0.0 - 1.0 scale.
  void RateCurrentSong(double rating);
  // Rate current song using 0 - 5 scale.
  void RateCurrentSong(int rating);

  void PlaySmartPlaylist(smart_playlists::GeneratorPtr generator, bool as_new, bool clear);

  void SongChangeRequestProcessed(const QUrl& url, bool valid);

private slots:
  void OneOfPlaylistsChanged();
  void UpdateSummaryText();
  void SongsDiscovered(const SongList& songs);
  void LoadFinished(bool success);

private:
  Playlist* AddPlaylist(int id, const QString& name, const QString& special_type);

private:
  struct Data {
    Data(Playlist* _p = NULL, const QString& _name = QString()) : p(_p), name(_name) {}
    Playlist* p;
    QString name;
    QItemSelection selection;
  };

  TaskManager* task_manager_;
  PlaylistBackend* playlist_backend_;
  LibraryBackend* library_backend_;
  PlaylistSequence* sequence_;
  PlaylistParser* parser_;
  PlaylistContainer* playlist_container_;

  // key = id
  QMap<int, Data> playlists_;

  QMap<QString, SpecialPlaylistType*> special_playlist_types_;
  SpecialPlaylistType* default_playlist_type_;

  int current_;
  int active_;
};

#endif // PLAYLISTMANAGER_H
