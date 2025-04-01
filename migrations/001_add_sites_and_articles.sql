-- 001_add_sites_and_articles.sql

-- Create subsite registry
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    slug TEXT UNIQUE NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    theme_config JSON
);

-- Add site_id to content_blocks (if not exists)
-- SQLite does not support IF NOT EXISTS for ALTER TABLE ADD COLUMN directly
-- So this step must be done conditionally in Bash (see below)

-- Rich text article storage
CREATE TABLE IF NOT EXISTS articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    content_id INTEGER NOT NULL REFERENCES content_blocks(id) ON DELETE CASCADE,
    body_markdown TEXT NOT NULL,
    summary TEXT,
    author TEXT,
    published_at DATETIME,
    last_edited DATETIME
);

-- Tags support
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL
);

CREATE TABLE IF NOT EXISTS content_tags (
    content_id INTEGER REFERENCES content_blocks(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (content_id, tag_id)
);
