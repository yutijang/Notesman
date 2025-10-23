PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS resources (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT NOT NULL,
    type        TEXT NOT NULL,   -- Ví dụ: 'text', 'cpp', 'pdf', 'epub'
	file_hash   TEXT UNIQUE NULL,     -- Kiểm tra trùng lặp file
    created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
	UNIQUE (title, type)
);

CREATE TABLE IF NOT EXISTS text_content (
    resource_id INTEGER PRIMARY KEY,
    content     TEXT NOT NULL,
    FOREIGN KEY (resource_id) REFERENCES resources(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS files (
    resource_id   INTEGER PRIMARY KEY,
    stored_path   TEXT,
    original_path TEXT NOT NULL,
    is_managed    INTEGER NOT NULL DEFAULT 0, -- 0 = linked, 1 = copied
    FOREIGN KEY (resource_id) REFERENCES resources(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS tags (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT UNIQUE NOT NULL COLLATE NOCASE
);

CREATE TABLE IF NOT EXISTS resource_tags (
    resource_id INTEGER,
    tag_id      INTEGER,
    PRIMARY KEY (resource_id, tag_id),
    FOREIGN KEY (resource_id) REFERENCES resources(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- -- --
CREATE VIRTUAL TABLE text_content_fts USING fts5(
    content,
    tokenize = 'unicode61 remove_diacritics 1'
);
-- -- --

-- Trigger khi INSERT
CREATE TRIGGER IF NOT EXISTS text_content_insert_fts
AFTER INSERT ON text_content
WHEN new.content IS NOT NULL
BEGIN
    INSERT INTO text_content_fts (rowid, content) VALUES (new.resource_id, new.content);
END;

-- Trigger khi UPDATE
CREATE TRIGGER IF NOT EXISTS text_content_update_fts
AFTER UPDATE ON text_content
BEGIN
    UPDATE text_content_fts SET content = new.content WHERE rowid = old.resource_id;
END;

-- Trigger khi DELETE
CREATE TRIGGER IF NOT EXISTS text_content_delete_fts
AFTER DELETE ON text_content
BEGIN
    DELETE FROM text_content_fts WHERE rowid = old.resource_id;
END;

-- -- --
-- Indexes cho bảng resources
CREATE INDEX IF NOT EXISTS idx_resources_title ON resources(title);
CREATE INDEX IF NOT EXISTS idx_resources_type ON resources(type);
CREATE UNIQUE INDEX IF NOT EXISTS idx_resources_title_type ON resources(title, type);

-- Index cho bảng tags
CREATE INDEX IF NOT EXISTS idx_tags_name ON tags(name);

-- Index cho bảng liên kết nhiều-nhiều resource_tags
CREATE INDEX IF NOT EXISTS idx_resource_tags_resource_id ON resource_tags(resource_id);
CREATE INDEX IF NOT EXISTS idx_resource_tags_tag_id ON resource_tags(tag_id);

-- -- --

-- Tạo bảng ảo FTS5 cho resources(title)
CREATE VIRTUAL TABLE IF NOT EXISTS resources_fts USING fts5(
    title,
    tokenize = 'unicode61 remove_diacritics 1'
);

-- Trigger khi INSERT vào resources
CREATE TRIGGER IF NOT EXISTS resources_insert_fts
AFTER INSERT ON resources
WHEN new.title IS NOT NULL
BEGIN
    INSERT INTO resources_fts (rowid, title) VALUES (new.id, new.title);
END;

-- Trigger khi UPDATE trên resources
CREATE TRIGGER IF NOT EXISTS resources_update_fts
AFTER UPDATE OF title ON resources
WHEN new.title IS NOT NULL
BEGIN
    UPDATE resources_fts SET title = new.title WHERE rowid = old.id;
END;

-- Trigger khi DELETE trên resources
CREATE TRIGGER IF NOT EXISTS resources_delete_fts
AFTER DELETE ON resources
BEGIN
    DELETE FROM resources_fts WHERE rowid = old.id;
END;

-- -- --

CREATE TRIGGER IF NOT EXISTS update_resource_timestamp
AFTER UPDATE ON resources
FOR EACH ROW
WHEN NEW.updated_at = OLD.updated_at
BEGIN
    UPDATE resources SET updated_at = CURRENT_TIMESTAMP WHERE id = OLD.id;
END;

