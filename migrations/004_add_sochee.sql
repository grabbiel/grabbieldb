CREATE TABLE IF NOT EXISTS sochee (
 id INTEGER PRIMARY KEY REFERENCES content_blocks(id) ON DELETE CASCADE,
 single BOOLEAN NOT NULL,
 comments INTEGER DEFAULT 0,
 likes INTEGER DEFAULT 0,
 caption TEXT NOT NULL, 
 hashtag INTEGER NOT NULL
 location TEXT NOT NULL DEFAULT '',
 has_link BOOLEAN NOT NULL
);
CREATE TABLE IF NOT EXISTS sochee_comment (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  content_id INTEGER NOT NULL REFERENCES sochee(id) ON DELETE CASCADE,
  embedded BOOLEAN NOT NULL,
  content TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS sochee_comment_embedded(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  comment_id INTEGER NOT NULL REFERENCES sochee_comment(id) ON DELETE CASCADE,
  x_coord INTEGER NOT NULL,
  y_coord INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS sochee_hashtag(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  content_id INTEGER NOT NULL REFERENCES sochee(id) ON DELETE CASCADE,
  hashtag TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS sochee_link(
  id INTEGER PRIMARY KEY REFERENCES sochee(id) ON DELETE CASCADE,
  image_id INTEGER NOT NULL REFERENCES images(id) ON DELETE CASCADE,
  url TEXT NOT NULL,
  name TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS sochee_order(
  id INT PRIMARY KEY REFERENCES images(id) ON DELETE CASCADE,
  sochee_id INTEGER NOT NULL REFERENCES sochee(id) ON DELETE CASCADE,
  photo_order INT NOT NULL
);
