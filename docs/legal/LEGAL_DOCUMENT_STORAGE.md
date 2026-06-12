# Hukuki Metin Saklama ve Sürümleme

## Kaynak Dosyalar

Hukuki metinlerin tam içeriği veritabanında değil, sürümlü Markdown dosyaları
olarak repository içinde tutulur:

- `clients/web/src/legal/documents/user-agreement.alpha-1.tr.md`
- `clients/web/src/legal/documents/privacy-notice.alpha-1.tr.md`

Dosya adları belge türünü, sürümü ve dili açıkça belirtir. Signup modalı bu
dosyaları doğrudan render eder.

## Hash ve Veritabanı Kaydı

Her belgenin normalize edilmiş dosya baytları için SHA-256 özeti
`clients/web/src/legal/constants.ts` içinde tutulur. Production build sırasında
hash sabitleri ile dosyaların gerçek hash değerleri karşılaştırılır.

Veritabanı hukuki metnin tam içeriğini saklamaz. Yalnızca kullanıcı, sürüm,
locale, belge hash'i ve kabul/teslim zamanı gibi ispat kayıtlarını saklar:

- `kergit_app.legal_terms_acceptances`
- `kergit_app.privacy_notice_deliveries`

Gizlilik Politikası / KVKK Aydınlatma Metni kaydı rıza veya kabul değildir;
bildirimin teslim edildiğini kaydeder.

## Yeni Sürüme Geçiş

1. Yeni metni, yeni sürümü dosya adında taşıyan ayrı bir Markdown dosyasına
   kopyalayın.
2. `clients/web/src/legal/constants.ts` içindeki ilgili sürüm sabitini
   güncelleyin.
3. Normalize edilmiş yeni dosyanın SHA-256 hash'ini hesaplayıp ilgili hash
   sabitini güncelleyin.
4. Signup modalının yeni dosyayı kullandığını doğrulayın.
5. Yeni bir DB migration ile trigger içindeki güncel sürüm/hash doğrulamasını
   ve kayıt davranışını güncelleyin.
6. Kullanıcı Sözleşmesi sürümü değiştiğinde kullanıcıdan yeni sürüm için tekrar
   kabul alın.
7. Gizlilik/KVKK metni değiştiğinde yeni sürümün kullanıcıya teslim edildiğine
   ilişkin yeni kayıt oluşturun; bunu rıza olarak adlandırmayın.

Eski sürümlü Markdown dosyalarını, geçmişte kabul edilen veya teslim edilen
metnin ispatlanabilmesi için repository geçmişi dışında da erişilebilir tutmak
hukuki/operasyonel bir karardır.
