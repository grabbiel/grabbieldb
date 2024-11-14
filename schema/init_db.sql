--- Database schema for content management
CREATE TABLE IF NOT EXISTS articles (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  title TEXT NOT NULL,
  content TEXT NOT NULL,
  category TEXT NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  published BOOLEAN DEFAULT 0
);

CREATE TABLE IF NOT EXISTS schema_versions (
  version INTEGER PRIMARY KEY,
  applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  description TEXT
);

--- Insert initial version
INSERT OR IGNORE INTO schema_versions (version, description)
VALUES(1, 'Initial schema');

--- Add indices for common queries
CREATE INDEX IF NOT EXISTS idx_articles_category ON articles(category);
CREATE INDEX IF NOT EXISTS idx_articles_published ON article(published);

--- View for published articles
CREATE VIEW IF NOT EXISTS published_articles AS 
SELECT * FROM articles WHERE published = 1;

--- Triggers for updated_at
CREATE TRIGGER IF NOT EXISTS articles_updated_at
  AFTER UPDATE ON articles
BEGIN
  UPDATE articles SET updated_at = CURRENT_TIMESTAMP
  WHERE id = NEW.id;
END;
