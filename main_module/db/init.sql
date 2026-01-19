-- Курсы
CREATE TABLE IF NOT EXISTS courses (
    id                      SERIAL PRIMARY KEY,
    title                   TEXT NOT NULL UNIQUE,
    description             TEXT,
    author_id               TEXT NOT NULL, 
    is_deleted              BOOLEAN DEFAULT FALSE,
    created_at              TIMESTAMP DEFAULT now()
);
CREATE TABLE IF NOT EXISTS course_students (
    course_id               INT REFERENCES courses(id) ON DELETE CASCADE,
    user_id                 TEXT NOT NULL,
    PRIMARY KEY (course_id, user_id)
);

CREATE SEQUENCE IF NOT EXISTS questions_id_seq;

-- Вопросы
CREATE TABLE IF NOT EXISTS questions (
    id                      INTEGER NOT NULL,
    version                 INTEGER NOT NULL,
    author_id               TEXT NOT NULL,
    title                   TEXT NOT NULL,
    content                 TEXT NOT NULL,
    options                 JSONB NOT NULL,
    correct_option          INTEGER NOT NULL,
    is_deleted              BOOLEAN DEFAULT false,
    PRIMARY KEY (id, version)
);

-- Тесты
CREATE TABLE IF NOT EXISTS tests (
    id                      SERIAL PRIMARY KEY,
    course_id               INT REFERENCES courses(id) ON DELETE CASCADE,
    title                   TEXT NOT NULL,
    question_ids            INTEGER[] DEFAULT '{}',
    is_active               BOOLEAN DEFAULT FALSE,
    is_deleted              BOOLEAN DEFAULT FALSE,
    author_id               TEXT NOT NULL
);


-- Попытки и ответы
CREATE TABLE IF NOT EXISTS test_attempts (
    id                      SERIAL PRIMARY KEY,
    user_id                 TEXT NOT NULL,
    test_id                 INTEGER REFERENCES tests(id) ON DELETE CASCADE,
    questions_snapshot      JSONB NOT NULL,
    user_answers            JSONB NOT NULL,
    status                  TEXT DEFAULT 'in_progress',
    score                   DOUBLE PRECISION DEFAULT 0.0,
    created_at              TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, test_id)
);


-- Уведомления
CREATE TABLE IF NOT EXISTS notifications (
    id                      SERIAL PRIMARY KEY,
    user_id                 TEXT NOT NULL,
    type                    TEXT NOT NULL,
    title                   TEXT,
    message                 TEXT NOT NULL,
    payload                 JSONB DEFAULT '{}',
    is_sent_tg              BOOLEAN DEFAULT FALSE,
    created_at              TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
