--- Content types table 
CREATE TABLE IF NOT EXISTS content_types (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT UNIQUE NOT NULL  -- 'article', 'minisite', 'gallery', 'interactive'
);
-- Main content blocks table
CREATE TABLE IF NOT EXISTS content_blocks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    url_slug TEXT UNIQUE NOT NULL,
    type_id INTEGER NOT NULL,
    status TEXT CHECK(status IN ('draft', 'published', 'archived')) DEFAULT 'draft',
    thumbnail_url TEXT,
    language TEXT DEFAULT 'en' NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(type_id) REFERENCES content_types(id)
);
-- Content metadata (flexible key-value store for additional properties)
CREATE TABLE IF NOT EXISTS content_metadata (
    content_id INTEGER NOT NULL,
    key TEXT NOT NULL,
    value TEXT,
    FOREIGN KEY(content_id) REFERENCES content_blocks(id) ON DELETE CASCADE,
    PRIMARY KEY(content_id, key)
);

-- Content files (tracks all files associated with a content block)
CREATE TABLE IF NOT EXISTS content_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    content_id INTEGER NOT NULL,
    file_type TEXT NOT NULL,  -- 'html', 'css', 'js', 'md', etc.
    file_path TEXT NOT NULL,
    is_main BOOLEAN DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(content_id) REFERENCES content_blocks(id) ON DELETE CASCADE
);

-- Images table for managing all images
CREATE TABLE IF NOT EXISTS images (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    original_url TEXT NOT NULL,
    filename TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    size INTEGER,  -- in bytes
    width INTEGER,
    height INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    content_id INTEGER,
    image_type TEXT CHECK(image_type IN ('thumbnail', 'content')) NOT NULL,
    processing_status TEXT CHECK(processing_status IN ('pending', 'processing', 'complete', 'error')) DEFAULT 'pending',
    FOREIGN KEY(content_id) REFERENCES content_blocks(id) ON DELETE CASCADE
);

-- Image variants for responsive images
CREATE TABLE IF NOT EXISTS image_variants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    image_id INTEGER NOT NULL,
    url TEXT NOT NULL,
    format TEXT NOT NULL,  -- 'webp', 'jpeg', 'avif'
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    quality TEXT CHECK(quality IN ('low', 'medium', 'high')) NOT NULL,
    viewport_size TEXT NOT NULL,  -- 'small', 'medium', 'large'
    size INTEGER,  -- in bytes
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(image_id) REFERENCES images(id) ON DELETE CASCADE
);

-- Videos table
CREATE TABLE IF NOT EXISTS videos (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    gcs_path TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    size_bytes INTEGER,
    duration_seconds INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    content_id INTEGER,
    processing_status TEXT CHECK(processing_status IN ('pending', 'processing', 'complete', 'error')) DEFAULT 'pending',
    FOREIGN KEY(content_id) REFERENCES content_blocks(id)
);

-- Video variants for different qualities
CREATE TABLE IF NOT EXISTS video_variants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    video_id INTEGER NOT NULL,
    quality TEXT NOT NULL, -- '360p', '720p', '1080p'
    format TEXT NOT NULL, -- 'mp4', 'webm'
    gcs_path TEXT NOT NULL,
    size_bytes INTEGER,
    segment_duration INTEGER DEFAULT 10, -- for adaptive streaming, in seconds
    FOREIGN KEY(video_id) REFERENCES videos(id) ON DELETE CASCADE
);

-- Create indexes for common queries
CREATE INDEX IF NOT EXISTS idx_content_blocks_status ON content_blocks(status);
CREATE INDEX IF NOT EXISTS idx_content_blocks_type ON content_blocks(type_id);
CREATE INDEX IF NOT EXISTS idx_content_blocks_url_slug ON content_blocks(url_slug);
CREATE INDEX IF NOT EXISTS idx_images_content ON images(content_id);
CREATE INDEX IF NOT EXISTS idx_videos_content ON videos(content_id);

-- Create updated_at triggers
CREATE TRIGGER IF NOT EXISTS content_blocks_updated_at 
    AFTER UPDATE ON content_blocks
BEGIN
    UPDATE content_blocks SET updated_at = CURRENT_TIMESTAMP
    WHERE id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS content_files_updated_at 
    AFTER UPDATE ON content_files
BEGIN
    UPDATE content_files SET updated_at = CURRENT_TIMESTAMP
    WHERE id = NEW.id;
END;

--- versioning table 
CREATE TABLE IF NOT EXISTS schema_versions (
  version INTEGER PRIMARY KEY,
  applied_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  description TEXT
);

--- Insert initial version
INSERT OR IGNORE INTO schema_versions (version, description)
VALUES(1, 'Initial schema');

-- Subsite registry
CREATE TABLE IF NOT EXISTS sites (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    slug TEXT UNIQUE NOT NULL,       -- e.g. 'leetcode', 'food'
    title TEXT NOT NULL,             -- Display name for subsite
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    theme_config JSON                -- Optional styling/theming info
);

-- Link content_blocks to subsites
ALTER TABLE content_blocks ADD COLUMN site_id INTEGER REFERENCES sites(id);

-- Rich text article table (optional)
CREATE TABLE IF NOT EXISTS articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    content_id INTEGER NOT NULL REFERENCES content_blocks(id) ON DELETE CASCADE,
    body_markdown TEXT NOT NULL,
    summary TEXT,
    author TEXT,
    published_at DATETIME,
    last_edited DATETIME
);

-- Optional tag support
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL
);
CREATE TABLE IF NOT EXISTS content_tags (
    content_id INTEGER REFERENCES content_blocks(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (content_id, tag_id)
);


