/* This file is part of Clementine.
   Copyright 2011, David Sansome <me@davidsansome.com>

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

#ifndef GROOVESHARKSEARCHPROVIDER_H
#define GROOVESHARKSEARCHPROVIDER_H

#include "searchprovider.h"
#include "core/backgroundthread.h"

class AlbumCoverLoader;
class GroovesharkService;

class GroovesharkSearchProvider : public SearchProvider {
  Q_OBJECT

 public:
  explicit GroovesharkSearchProvider(QObject* parent = 0);
  void Init(GroovesharkService* service);

  // SearchProvider
  void SearchAsync(int id, const QString& query);
  void LoadArtAsync(int id, const Result& result);
  void LoadTracksAsync(int id, const Result& result);
  bool IsLoggedIn();
  void ShowConfig();

 private slots:
  void SearchDone(int id, const SongList& songs);
  void AlbumSearchResult(int id, const SongList& songs);
  void AlbumArtLoaded(quint64 id, const QImage& image);
  void AlbumSongsLoaded(int id, const SongList& songs);

 private:
  void MaybeSearchFinished(int id);
  void FetchAlbum(int id, const Result& result);

  GroovesharkService* service_;
  QMap<int, PendingState> pending_searches_;

  BackgroundThread<AlbumCoverLoader>* cover_loader_;
  QMap<quint64, int> cover_loader_tasks_;
};

#endif
