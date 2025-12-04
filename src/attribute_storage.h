#pragma once

#include <memory>
#include <ogrsf_frmts.h>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief RAII wrapper for SQLite attribute storage
 *
 * This class manages the lifecycle of SQLite database connection and provides
 * efficient batch insertion of OGR feature attributes.
 */
class AttributeStorage {
public:
  /**
   * @brief Construct and open/create SQLite database
   * @param db_path Path to SQLite database file
   */
  explicit AttributeStorage(const std::string &db_path);

  /**
   * @brief Destructor - automatically closes database and commits pending
   * transactions
   */
  ~AttributeStorage();

  // Disable copy
  AttributeStorage(const AttributeStorage &) = delete;
  AttributeStorage &operator=(const AttributeStorage &) = delete;

  // Enable move
  AttributeStorage(AttributeStorage &&other) noexcept;
  AttributeStorage &operator=(AttributeStorage &&other) noexcept;

  /**
   * @brief Check if database is open and ready
   */
  bool isOpen() const { return db_ != nullptr; }

  /**
   * @brief Create table schema from OGR layer definition
   * @param layer_defn OGR layer definition containing field information
   * @return true if table created successfully
   */
  bool createTable(OGRFeatureDefn *layer_defn);

  /**
   * @brief Insert a single feature's attributes
   * @param feature OGR feature to extract attributes from
   * @param layer_defn Layer definition for field metadata
   * @return true if insertion successful
   */
  bool insertFeature(OGRFeature *feature, OGRFeatureDefn *layer_defn);

  /**
   * @brief Begin a transaction for batch operations
   * @return true if transaction started successfully
   */
  bool beginTransaction();

  /**
   * @brief Commit current transaction
   * @return true if commit successful
   */
  bool commit();

  /**
   * @brief Rollback current transaction
   * @return true if rollback successful
   */
  bool rollback();

  /**
   * @brief Insert multiple features in batches with auto-commit
   * @param layer OGR layer containing features
   * @param batch_size Number of features per transaction (default: 1000)
   * @return Number of successfully inserted features
   */
  size_t insertFeaturesInBatches(OGRLayer *layer, size_t batch_size = 1000);

  /**
   * @brief Get last error message
   */
  std::string getLastError() const { return last_error_; }

private:
  /**
   * @brief Execute SQL statement
   * @param sql SQL statement to execute
   * @return true if execution successful
   */
  bool executeSql(const std::string &sql);

  /**
   * @brief Map OGR field type to SQLite type name
   */
  static std::string mapFieldType(OGRFieldType type);

  /**
   * @brief Escape SQL string value
   */
  static std::string escapeSqlString(const std::string &str);

  /**
   * @brief Build INSERT SQL using stringstream for efficiency
   */
  std::string buildInsertSql(OGRFeature *feature, OGRFeatureDefn *layer_defn);

  /**
   * @brief Close database connection
   */
  void close();

private:
  sqlite3 *db_;
  std::string db_path_;
  std::string last_error_;
  bool in_transaction_;
  size_t pending_count_; // Track pending inserts in current transaction
};
