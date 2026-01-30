PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS resources (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    title       TEXT NOT NULL COLLATE NOCASE,
    type        TEXT NOT NULL,          -- 'text', 'cpp', 'pdf', 'epub'
    file_hash   TEXT UNIQUE NULL,       -- Kiểm tra trùng lặp file
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

CREATE TABLE IF NOT EXISTS resource_urls (
    resource_id    INTEGER PRIMARY KEY,

    url            TEXT NOT NULL,              -- URL gốc người dùng nhập
    normalized_url TEXT NOT NULL UNIQUE,       -- URL chuẩn hoá để chống trùng

    domain         TEXT NOT NULL,              -- ví dụ: w3schools.com
    url_path       TEXT,                       -- /cpp/array

    created_at     TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (resource_id)
        REFERENCES resources(id)
        ON DELETE CASCADE
);

-- -- --
CREATE VIRTUAL TABLE text_content_fts USING fts5(
    content,
    content='text_content',
    content_rowid='resource_id',    
    tokenize = 'unicode61 remove_diacritics 1'
);
-- -- --

-- Trigger INSERT cho text_content
CREATE TRIGGER IF NOT EXISTS text_content_ai
AFTER INSERT ON text_content
BEGIN
  INSERT INTO text_content_fts(rowid, content)
  VALUES (new.resource_id, new.content);
END;

-- Trigger UPDATE cho text_content
CREATE TRIGGER IF NOT EXISTS text_content_au
AFTER UPDATE OF content ON text_content
BEGIN
  INSERT INTO text_content_fts(text_content_fts, rowid, content)
  VALUES('delete', old.resource_id, old.content);

  INSERT INTO text_content_fts(rowid, content)
  VALUES(new.resource_id, new.content);
END;

-- Trigger DELETE cho text_content
CREATE TRIGGER IF NOT EXISTS text_content_ad
AFTER DELETE ON text_content
BEGIN
  INSERT INTO text_content_fts(text_content_fts, rowid, content)
  VALUES('delete', old.resource_id, old.content);
END;

-- -- --
-- Indexes cho bảng resources
CREATE INDEX IF NOT EXISTS idx_resources_type ON resources(type);

-- Index cho bảng tags
CREATE INDEX IF NOT EXISTS idx_tags_name ON tags(name);

-- Index cho bảng liên kết nhiều-nhiều resource_tags
CREATE INDEX IF NOT EXISTS idx_resource_tags_resource_id ON resource_tags(resource_id);
CREATE INDEX IF NOT EXISTS idx_resource_tags_tag_id ON resource_tags(tag_id);

-- Index cho domain (search chính xác)
CREATE INDEX IF NOT EXISTS idx_resource_urls_domain ON resource_urls(domain);

-- -- --

-- Tạo bảng ảo FTS5 cho resources(title)
CREATE VIRTUAL TABLE IF NOT EXISTS resources_fts USING fts5(
    title,
    content='resources',
    content_rowid='id',    
    tokenize = 'unicode61 remove_diacritics 1'
);

-- 1. Trigger INSERT: Đồng bộ khi thêm bản ghi mới
CREATE TRIGGER IF NOT EXISTS resources_ai
AFTER INSERT ON resources
BEGIN
  INSERT INTO resources_fts(rowid, title) 
  VALUES (new.id, new.title);
END;

-- 2. Trigger UPDATE: Xóa chỉ mục cũ và chèn chỉ mục mới (Chỉ chạy khi 'title' thay đổi)
CREATE TRIGGER IF NOT EXISTS resources_au
AFTER UPDATE OF title ON resources
BEGIN
  INSERT INTO resources_fts(resources_fts, rowid, title) 
  VALUES('delete', old.id, old.title);
  
  INSERT INTO resources_fts(rowid, title) 
  VALUES(new.id, new.title);
END;

-- 3. Trigger DELETE: Loại bỏ chỉ mục khi xóa bản ghi
CREATE TRIGGER IF NOT EXISTS resources_ad
AFTER DELETE ON resources
BEGIN
  INSERT INTO resources_fts(resources_fts, rowid, title) 
  VALUES('delete', old.id, old.title);
END;

-- -- --

CREATE TRIGGER IF NOT EXISTS update_resource_timestamp
AFTER UPDATE OF title, type, file_hash ON resources
FOR EACH ROW
WHEN NEW.updated_at = OLD.updated_at
BEGIN
    UPDATE resources
    SET updated_at = CURRENT_TIMESTAMP
    WHERE id = OLD.id;
END;

---

--- Trigger cập nhật resources.updated_at khi content đổi
CREATE TRIGGER IF NOT EXISTS text_content_touch_resource
AFTER UPDATE OF content ON text_content
FOR EACH ROW
WHEN COALESCE(NEW.content, '') <> COALESCE(OLD.content, '')
BEGIN
    UPDATE resources
    SET updated_at = CURRENT_TIMESTAMP
    WHERE id = NEW.resource_id;
END;

--- Bảng lưu nội dung text của file
CREATE TABLE IF NOT EXISTS file_text_content (
    resource_id INTEGER PRIMARY KEY,
    extracted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    content TEXT, -- cho trường hợp pdf và epub

    FOREIGN KEY (resource_id)
        REFERENCES resources(id)
        ON DELETE CASCADE
);

--- FTS5 cho file text (external content)
CREATE VIRTUAL TABLE file_text_content_fts USING fts5(
    content,
    content='file_text_content',
    content_rowid='resource_id',
    tokenize = 'unicode61 remove_diacritics 1'
);

--- Trigger đồng bộ FTS cho file text ---
--- INSERT
CREATE TRIGGER IF NOT EXISTS file_text_content_ai
AFTER INSERT ON file_text_content
BEGIN
  INSERT INTO file_text_content_fts(rowid, content)
  VALUES (new.resource_id, new.content);
END;

--- UPDATE
CREATE TRIGGER IF NOT EXISTS file_text_content_au
AFTER UPDATE OF content ON file_text_content
BEGIN
  INSERT INTO file_text_content_fts(file_text_content_fts, rowid, content)
  VALUES('delete', old.resource_id, old.content);

  INSERT INTO file_text_content_fts(rowid, content)
  VALUES(new.resource_id, new.content);
END;

--- DELETE
CREATE TRIGGER IF NOT EXISTS file_text_content_ad
AFTER DELETE ON file_text_content
BEGIN
  INSERT INTO file_text_content_fts(file_text_content_fts, rowid, content)
  VALUES('delete', old.resource_id, old.content);
END;

CREATE TRIGGER IF NOT EXISTS file_text_content_touch_resource
AFTER UPDATE OF content ON file_text_content
FOR EACH ROW
WHEN COALESCE(NEW.content, '') <> COALESCE(OLD.content, '')
BEGIN
    UPDATE resources
    SET updated_at = CURRENT_TIMESTAMP
    WHERE id = NEW.resource_id;
END;

---
-- FTS cho path (không cho domain)
CREATE VIRTUAL TABLE IF NOT EXISTS resource_url_path_fts USING fts5(
    url_path,
    content='resource_urls',
    content_rowid='resource_id',
    tokenize = 'unicode61 remove_diacritics 1'
);

-- Trigger đồng bộ
-- INSERT
CREATE TRIGGER IF NOT EXISTS resource_url_path_ai
AFTER INSERT ON resource_urls
BEGIN
  INSERT INTO resource_url_path_fts(rowid, url_path)
  VALUES (new.resource_id, new.url_path);
END;

-- UPDATE
CREATE TRIGGER IF NOT EXISTS resource_url_path_au
AFTER UPDATE OF url_path ON resource_urls
BEGIN
  INSERT INTO resource_url_path_fts(resource_url_path_fts, rowid, url_path)
  VALUES('delete', old.resource_id, old.url_path);

  INSERT INTO resource_url_path_fts(rowid, url_path)
  VALUES(new.resource_id, new.url_path);
END;

-- DELETE
CREATE TRIGGER IF NOT EXISTS resource_url_path_ad
AFTER DELETE ON resource_urls
BEGIN
  INSERT INTO resource_url_path_fts(resource_url_path_fts, rowid, url_path)
  VALUES('delete', old.resource_id, old.url_path);
END;

-- Trigger cập nhật updated_at
CREATE TRIGGER IF NOT EXISTS resource_urls_touch_resource
AFTER UPDATE OF url, normalized_url ON resource_urls
FOR EACH ROW
WHEN COALESCE(NEW.url,'') <> COALESCE(OLD.url,'')
  OR COALESCE(NEW.normalized_url,'') <> COALESCE(OLD.normalized_url,'')
BEGIN
  UPDATE resources
  SET updated_at = CURRENT_TIMESTAMP
  WHERE id = NEW.resource_id;
END;
