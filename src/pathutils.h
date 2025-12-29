/**
 * @file pathutils.h
 * @brief Cross-Platform Path Handling Utilities
 * 
 * CRITICAL FIX: Path Handling
 * 
 * PROBLEM:
 * - Manual "/" or "\\" concatenation breaks
 * - Windows UTF-16 vs POSIX UTF-8 issues
 * - "Works on my machine" syndrome
 * 
 * SOLUTION:
 * - Always use Qt path utilities
 * - QDir for path operations
 * - QStandardPaths for system locations
 * - UTF-8 everywhere in C core
 * 
 * USAGE:
 *   // ❌ WRONG:
 *   QString path = dir + "/" + file;
 *   
 *   // ✅ RIGHT:
 *   QString path = PathUtils::join(dir, file);
 *   QString path = PathUtils::toNative(somePath);
 */

#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

/**
 * @brief Cross-platform path utilities
 * 
 * All path operations are platform-aware and handle:
 * - Windows: backslashes, drive letters, UTF-16
 * - macOS/Linux: forward slashes, UTF-8
 */
class PathUtils
{
public:
    /**
     * @brief Join path components safely
     * @param parts Path components to join
     * @return Combined path with correct separators
     * 
     * @example
     * QString path = PathUtils::join({"C:", "Users", "Documents", "file.scp"});
     * // Windows: "C:/Users/Documents/file.scp"
     * // Linux: "C:/Users/Documents/file.scp" (or wherever root is)
     */
    static QString join(const QStringList& parts)
    {
        if (parts.isEmpty()) return QString();
        
        QString result = parts.first();
        for (int i = 1; i < parts.size(); ++i) {
            result = QDir(result).filePath(parts.at(i));
        }
        return QDir::cleanPath(result);
    }
    
    /**
     * @brief Join two path components
     * @param dir Directory path
     * @param file Filename or subdirectory
     * @return Combined path
     */
    static QString join(const QString& dir, const QString& file)
    {
        return QDir(dir).filePath(file);
    }
    
    /**
     * @brief Convert to native path separators
     * @param path Path with forward slashes
     * @return Path with platform-native separators
     * 
     * @example
     * QString native = PathUtils::toNative("C:/Users/file.txt");
     * // Windows: "C:\Users\file.txt"
     * // Linux: "C:/Users/file.txt"
     */
    static QString toNative(const QString& path)
    {
        return QDir::toNativeSeparators(path);
    }
    
    /**
     * @brief Convert to forward slashes (canonical form)
     * @param path Path with any separators
     * @return Path with forward slashes
     */
    static QString toCanonical(const QString& path)
    {
        return QDir::cleanPath(path);
    }
    
    /**
     * @brief Get absolute path
     * @param path Relative or absolute path
     * @return Absolute path
     */
    static QString absolute(const QString& path)
    {
        return QFileInfo(path).absoluteFilePath();
    }
    
    /**
     * @brief Check if path exists
     * @param path Path to check
     * @return true if exists
     */
    static bool exists(const QString& path)
    {
        return QFileInfo::exists(path);
    }
    
    /**
     * @brief Check if path is a directory
     * @param path Path to check
     * @return true if directory
     */
    static bool isDir(const QString& path)
    {
        return QFileInfo(path).isDir();
    }
    
    /**
     * @brief Check if path is a file
     * @param path Path to check
     * @return true if file
     */
    static bool isFile(const QString& path)
    {
        return QFileInfo(path).isFile();
    }
    
    /**
     * @brief Get filename from path
     * @param path Full path
     * @return Filename only
     * 
     * @example
     * QString name = PathUtils::filename("/path/to/file.scp");
     * // Returns: "file.scp"
     */
    static QString filename(const QString& path)
    {
        return QFileInfo(path).fileName();
    }
    
    /**
     * @brief Get directory from path
     * @param path Full path
     * @return Directory path
     * 
     * @example
     * QString dir = PathUtils::dirname("/path/to/file.scp");
     * // Returns: "/path/to"
     */
    static QString dirname(const QString& path)
    {
        return QFileInfo(path).absolutePath();
    }
    
    /**
     * @brief Get file extension
     * @param path Filename or path
     * @return Extension without dot
     * 
     * @example
     * QString ext = PathUtils::extension("file.scp");
     * // Returns: "scp"
     */
    static QString extension(const QString& path)
    {
        return QFileInfo(path).suffix().toLower();
    }
    
    /**
     * @brief Get filename without extension
     * @param path Filename or path
     * @return Base filename
     * 
     * @example
     * QString base = PathUtils::basename("file.scp");
     * // Returns: "file"
     */
    static QString basename(const QString& path)
    {
        return QFileInfo(path).completeBaseName();
    }
    
    /**
     * @brief Create directory (including parents)
     * @param path Directory to create
     * @return true if successful
     */
    static bool mkdirs(const QString& path)
    {
        return QDir().mkpath(path);
    }
    
    /**
     * @brief Get standard output directory for flux images
     * @return Platform-appropriate default output directory
     * 
     * Windows: C:/Users/USERNAME/Documents/UnifiedFloppyTool
     * macOS: ~/Documents/UnifiedFloppyTool
     * Linux: ~/Documents/UnifiedFloppyTool
     */
    static QString defaultOutputDir()
    {
        QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return join(docs, "UnifiedFloppyTool");
    }
    
    /**
     * @brief Get standard config directory
     * @return Platform-appropriate config directory
     * 
     * Windows: C:/Users/USERNAME/AppData/Local/UnifiedFloppyTool
     * macOS: ~/Library/Application Support/UnifiedFloppyTool
     * Linux: ~/.config/UnifiedFloppyTool
     */
    static QString configDir()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    
    /**
     * @brief Get standard cache directory
     * @return Platform-appropriate cache directory
     */
    static QString cacheDir()
    {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    
    /**
     * @brief Convert QString to UTF-8 for C core
     * @param path Qt path string
     * @return UTF-8 encoded std::string for C API
     * 
     * @note C core must expect UTF-8!
     */
    static std::string toUtf8(const QString& path)
    {
        return path.toUtf8().toStdString();
    }
    
    /**
     * @brief Create UTF-8 C string for C core (malloc'd)
     * @param path Qt path string
     * @return malloc'd UTF-8 string (caller must free!)
     * 
     * @warning Caller must call free() on returned pointer!
     */
    static char* toUtf8Cstr(const QString& path)
    {
        QByteArray utf8 = path.toUtf8();
        char* cstr = (char*)malloc(utf8.size() + 1);
        if (cstr) {
            memcpy(cstr, utf8.constData(), utf8.size());
            cstr[utf8.size()] = '\0';
        }
        return cstr;
    }
};

#endif // PATHUTILS_H
