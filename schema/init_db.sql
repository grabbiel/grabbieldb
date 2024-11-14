--- Database schema for content management
CREATE TABLE IF NOT EXISTS articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    category TEXT NOT NULL,
    tags TEXT DEFAULT '[]' NOT NULL, -- Store as JSON array
    url_slug TEXT UNIQUE NOT NULL,   -- Ensure unique URLs
    status TEXT CHECK(status IN ('draft', 'published', 'archived')) DEFAULT 'draft' NOT NULL,
    thumbnail_url TEXT,
    language TEXT DEFAULT 'en' NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Add indexes for common queries
CREATE INDEX IF NOT EXISTS idx_articles_status ON articles(status);
CREATE INDEX IF NOT EXISTS idx_articles_url_slug ON articles(url_slug);
CREATE INDEX IF NOT EXISTS idx_articles_category ON articles(category);

-- Update trigger for updated_at
CREATE TRIGGER IF NOT EXISTS articles_updated_at 
    AFTER UPDATE ON articles
BEGIN
    UPDATE articles SET updated_at = CURRENT_TIMESTAMP
    WHERE id = NEW.id;
END;

-- Create views for different status types
CREATE VIEW IF NOT EXISTS published_articles AS
SELECT * FROM articles WHERE status = 'published';

CREATE VIEW IF NOT EXISTS draft_articles AS
SELECT * FROM articles WHERE status = 'draft';

CREATE VIEW IF NOT EXISTS archived_articles AS
SELECT * FROM articles WHERE status = 'archived';

--- versioning table 
CREATE TABLE IF NOT EXISTS schema_versions (
  version INTEGER PRIMARY KEY,
  applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  description TEXT
);


--- Insert initial version
INSERT OR IGNORE INTO schema_versions (version, description)
VALUES(1, 'Initial schema');


