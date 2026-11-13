#ifndef APPINFO_H
#define APPINFO_H

/**
 * @brief Uygulama sürümü ve yayın deposu bilgileri.
 *
 * Yeni bir sürüm yayınlamadan önce Version artırılmalı ve GitHub'da aynı
 * numarayla "v" önekli bir etiket (ör. Version "1.1.0" → etiket "v1.1.0")
 * oluşturulmalıdır; güncelleme denetimi bu iki değeri karşılaştırır.
 */
namespace AppInfo {

/// Uygulamanın sürümü (semantik sürümleme: ana.ara.yama).
constexpr const char *Version = "1.1.0";

/// Yayınların bulunduğu GitHub deposunun sahibi.
constexpr const char *GitHubOwner = "berkermelih";

/// Yayınların bulunduğu GitHub deposu.
constexpr const char *GitHubRepository = "CAN-viewer";

} // namespace AppInfo

#endif // APPINFO_H
