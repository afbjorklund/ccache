const std::string CREATE_FILES_TABLE =
  "CREATE TABLE \"files\" (\n"
  "            \"path\"          VARCHAR(65535) NOT NULL,  /* path with leading and without trailing slash */\n"
  "            \"name\"          VARCHAR(65535) NOT NULL,\n"
  "            \"offsetheader\"  INTEGER,  /* seek offset from TAR file where the TAR metadata for this file resides */\n"
  "            \"offset\"        INTEGER,  /* seek offset from TAR file where these file's contents resides */\n"
  "            \"size\"          INTEGER,\n"
  "            \"mtime\"         REAL,\n"
  "            \"mode\"          INTEGER,\n"
  "            \"type\"          INTEGER,\n"
  "            \"linkname\"      VARCHAR(65535),\n"
  "            \"uid\"           INTEGER,\n"
  "            \"gid\"           INTEGER,\n"
  "            /* True for valid TAR files. Internally used to determine where to mount recursive TAR files. */\n"
  "            \"istar\"         BOOL   ,\n"
  "            \"issparse\"      BOOL   ,  /* for sparse files the file size refers to the expanded size! */\n"
  "            /* See SQL benchmarks for decision on the primary key.\n"
  "             * See also https://www.sqlite.org/optoverview.html\n"
  "             * (path,name) tuples might appear multiple times in a TAR if it got updated.\n"
  "             * In order to also be able to show older versions, we need to add\n"
  "             * the offsetheader column to the primary key. */\n"
  "            PRIMARY KEY (path,name,offsetheader)\n"
  ");";

// return the files last added first
const std::string SELECT_FILES_TABLE =
  "SELECT * FROM \"files\" "
  "WHERE \"path\" == (?) AND \"name\" == (?) "
  "ORDER BY \"offsetheader\" DESC";

// "istar" and "issparse" are unused
const std::string INSERT_FILES_TABLE =
  "INSERT INTO \"files\" VALUES("
  "?,?,?,?,?,?,?,?,?,?,?,FALSE,FALSE"
  ");";
