#include "attribute_storage.h"
#include "extern.h"
#include <algorithm>
#include <cstdio>

// Constructor
AttributeStorage::AttributeStorage(const std::string &db_path)
    : db_(nullptr), db_path_(db_path), in_transaction_(false),
      pending_count_(0) {
  int rc = sqlite3_open(db_path.c_str(), &db_);
  if (rc != SQLITE_OK) {
    last_error_ = "Cannot create SQLite database: ";
    if (db_) {
      last_error_ += sqlite3_errmsg(db_);
    }
    close();
  }
}

// Destructor - RAII cleanup
AttributeStorage::~AttributeStorage() {
  // Commit any pending transaction before closing
  if (in_transaction_) {
    commit();
  }
  close();
}

// Move constructor
AttributeStorage::AttributeStorage(AttributeStorage &&other) noexcept
    : db_(other.db_), db_path_(std::move(other.db_path_)),
      last_error_(std::move(other.last_error_)),
      in_transaction_(other.in_transaction_),
      pending_count_(other.pending_count_) {
  other.db_ = nullptr;
  other.in_transaction_ = false;
  other.pending_count_ = 0;
}

// Move assignment
AttributeStorage &
AttributeStorage::operator=(AttributeStorage &&other) noexcept {
  if (this != &other) {
    // Clean up existing resources
    if (in_transaction_) {
      commit();
    }
    close();

    // Transfer ownership
    db_ = other.db_;
    db_path_ = std::move(other.db_path_);
    last_error_ = std::move(other.last_error_);
    in_transaction_ = other.in_transaction_;
    pending_count_ = other.pending_count_;

    other.db_ = nullptr;
    other.in_transaction_ = false;
    other.pending_count_ = 0;
  }
  return *this;
}

// Close database connection
void AttributeStorage::close() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

// Execute SQL statement
bool AttributeStorage::executeSql(const std::string &sql) {
  if (!db_) {
    last_error_ = "Database not open";
    return false;
  }

  char *err_msg = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);

  if (rc != SQLITE_OK) {
    last_error_ = "SQL error: ";
    if (err_msg) {
      last_error_ += err_msg;
      sqlite3_free(err_msg);
    }
    return false;
  }

  return true;
}

// Map OGR field type to SQLite type
std::string AttributeStorage::mapFieldType(OGRFieldType type) {
  switch (type) {
  case OFTInteger:
  case OFTInteger64:
    return "INTEGER";
  case OFTReal:
    return "REAL";
  case OFTString:
  default:
    return "TEXT";
  }
}

// Escape SQL string value
std::string AttributeStorage::escapeSqlString(const std::string &str) {
  std::string result;
  result.reserve(str.size() + 10); // Reserve space to avoid reallocation

  for (char ch : str) {
    if (ch == '\'') {
      result += "''"; // Escape single quote
    } else {
      result += ch;
    }
  }

  return result;
}

// Create table from layer definition
bool AttributeStorage::createTable(OGRFeatureDefn *layer_defn) {
  if (!db_ || !layer_defn) {
    last_error_ = "Invalid database or layer definition";
    return false;
  }

  // Use stringstream for efficient string building
  std::ostringstream sql;
  sql << "CREATE TABLE IF NOT EXISTS feature_attributes (fid INTEGER PRIMARY "
         "KEY";

  int field_count = layer_defn->GetFieldCount();
  for (int i = 0; i < field_count; ++i) {
    OGRFieldDefn *field_defn = layer_defn->GetFieldDefn(i);
    sql << ", " << field_defn->GetNameRef() << " "
        << mapFieldType(field_defn->GetType());
  }

  sql << ");";

  return executeSql(sql.str());
}

// Build INSERT SQL efficiently
std::string AttributeStorage::buildInsertSql(OGRFeature *feature,
                                             OGRFeatureDefn *layer_defn) {
  std::ostringstream sql;
  sql << "INSERT OR REPLACE INTO feature_attributes (fid";

  int field_count = layer_defn->GetFieldCount();

  // Build column names
  for (int i = 0; i < field_count; ++i) {
    sql << ", " << layer_defn->GetFieldDefn(i)->GetNameRef();
  }

  sql << ") VALUES (" << feature->GetFID();

  // Build values
  for (int i = 0; i < field_count; ++i) {
    sql << ", ";

    if (feature->IsFieldNull(i)) {
      sql << "NULL";
    } else {
      OGRFieldType field_type = layer_defn->GetFieldDefn(i)->GetType();

      switch (field_type) {
      case OFTInteger:
        sql << feature->GetFieldAsInteger(i);
        break;
      case OFTInteger64:
        sql << feature->GetFieldAsInteger64(i);
        break;
      case OFTReal:
        sql << feature->GetFieldAsDouble(i);
        break;
      case OFTString:
      default:
        sql << "'" << escapeSqlString(feature->GetFieldAsString(i)) << "'";
        break;
      }
    }
  }

  sql << ");";
  return sql.str();
}

// Insert single feature
bool AttributeStorage::insertFeature(OGRFeature *feature,
                                     OGRFeatureDefn *layer_defn) {
  if (!db_ || !feature || !layer_defn) {
    last_error_ = "Invalid parameters";
    return false;
  }

  std::string sql = buildInsertSql(feature, layer_defn);
  bool success = executeSql(sql);

  if (success && in_transaction_) {
    ++pending_count_;
  }

  return success;
}

// Begin transaction
bool AttributeStorage::beginTransaction() {
  if (!db_) {
    last_error_ = "Database not open";
    return false;
  }

  if (in_transaction_) {
    last_error_ = "Transaction already in progress";
    return false;
  }

  if (executeSql("BEGIN TRANSACTION;")) {
    in_transaction_ = true;
    pending_count_ = 0;
    return true;
  }

  return false;
}

// Commit transaction
bool AttributeStorage::commit() {
  if (!db_) {
    last_error_ = "Database not open";
    return false;
  }

  if (!in_transaction_) {
    // No transaction to commit is not an error
    return true;
  }

  bool success = executeSql("COMMIT;");
  if (success) {
    in_transaction_ = false;
    pending_count_ = 0;
  }

  return success;
}

// Rollback transaction
bool AttributeStorage::rollback() {
  if (!db_) {
    last_error_ = "Database not open";
    return false;
  }

  if (!in_transaction_) {
    return true;
  }

  bool success = executeSql("ROLLBACK;");
  if (success) {
    in_transaction_ = false;
    pending_count_ = 0;
  }

  return success;
}

// Insert features in batches with auto-commit
size_t AttributeStorage::insertFeaturesInBatches(OGRLayer *layer,
                                                 size_t batch_size) {
  if (!db_ || !layer) {
    last_error_ = "Invalid database or layer";
    return 0;
  }

  OGRFeatureDefn *layer_defn = layer->GetLayerDefn();
  if (!layer_defn) {
    last_error_ = "Invalid layer definition";
    return 0;
  }

  layer->ResetReading();

  size_t total_inserted = 0;
  size_t batch_count = 0;
  bool transaction_started = false;

  OGRFeature *feature = nullptr;

  while ((feature = layer->GetNextFeature()) != nullptr) {
    // Start transaction for each batch
    if (batch_count == 0) {
      if (!beginTransaction()) {
        LOG_E("Failed to begin transaction: %s", last_error_.c_str());
        OGRFeature::DestroyFeature(feature);
        break;
      }
      transaction_started = true;
    }

    // Insert feature
    if (insertFeature(feature, layer_defn)) {
      ++batch_count;
      ++total_inserted;
    } else {
      LOG_E("Failed to insert feature %lld: %s", (long long)feature->GetFID(),
            last_error_.c_str());
    }

    OGRFeature::DestroyFeature(feature);

    // Commit batch when reaching batch_size
    if (batch_count >= batch_size) {
      if (!commit()) {
        LOG_E("Failed to commit batch: %s", last_error_.c_str());
        rollback(); // Rollback on commit failure
        break;
      }
      transaction_started = false;
      batch_count = 0;
    }
  }

  // Commit remaining features
  if (transaction_started && batch_count > 0) {
    if (!commit()) {
      LOG_E("Failed to commit final batch: %s", last_error_.c_str());
      rollback();
    }
  }

  printf("Successfully stored %zu features' attributes to %s\n", total_inserted,
         db_path_.c_str());

  return total_inserted;
}
