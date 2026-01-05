# UFT Release Process

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Release-Typen

| Typ | Frequenz | Beispiel | Zweck |
|-----|----------|----------|-------|
| Major | Jährlich | 3.0.0 | Breaking Changes |
| Minor | Monatlich | 2.10.0 | Neue Features |
| Patch | Bei Bedarf | 2.9.1 | Bug-Fixes |
| Security | Sofort | 2.9.1-sec | Sicherheits-Fixes |

---

## 2. Pre-Release Checklist

### 2.1 Code-Freeze (1 Woche vor Release)

- [ ] Feature-Freeze: Keine neuen Features
- [ ] Nur Bug-Fixes und Dokumentation
- [ ] Release-Branch erstellen: `release/v2.9.0`

### 2.2 Testing (3 Tage vor Release)

- [ ] Alle CI-Workflows grün
- [ ] Test-Coverage ≥ 70%
- [ ] Keine Critical/High Bugs offen
- [ ] Manuelle Tests auf allen Tier-1 Plattformen
- [ ] Upgrade-Test von vorheriger Version

### 2.3 Documentation (2 Tage vor Release)

- [ ] CHANGELOG.md aktualisiert
- [ ] README.md aktuell
- [ ] API-Dokumentation generiert
- [ ] Migration Guide (bei Breaking Changes)
- [ ] Release Notes geschrieben

### 2.4 Final Checks (1 Tag vor Release)

- [ ] Version-Bump in CMakeLists.txt
- [ ] Version-Bump in uft_version.h
- [ ] Alle Links in Dokumentation funktionieren
- [ ] Lizenz-Header aktuell
- [ ] Copyright-Jahr aktuell

---

## 3. Release-Workflow

### 3.1 Git-Workflow

```bash
# 1. Release-Branch erstellen
git checkout develop
git checkout -b release/v2.9.0

# 2. Version-Bump
# CMakeLists.txt, uft_version.h bearbeiten
git commit -am "Bump version to 2.9.0"

# 3. Release-Tag erstellen
git tag -a v2.9.0 -m "Release 2.9.0"

# 4. In main und develop mergen
git checkout main
git merge release/v2.9.0
git push origin main --tags

git checkout develop
git merge release/v2.9.0
git push origin develop

# 5. Release-Branch löschen
git branch -d release/v2.9.0
```

### 3.2 Automatischer Build

Bei Tag-Push baut CI automatisch:
- Linux x86_64 Binary (tar.gz)
- macOS x86_64 + ARM64 Universal Binary (dmg)
- Windows x64 Binary (zip, msi)

---

## 4. Release auf GitHub

### 4.1 Release Notes Template

```markdown
# UFT v2.9.0

**Release Date:** 2026-01-15

## Highlights

- 🔒 Hardened parsers for improved security
- 🚀 30% faster MFM decoding with AVX2
- 🆕 Full HFE v3 support

## What's Changed

### New Features
- Feature 1 (#123)
- Feature 2 (#124)

### Bug Fixes
- Fix issue X (#125)
- Fix issue Y (#126)

### Security
- CVE-2026-XXXX: Fixed integer overflow

## Breaking Changes

None in this release.

## Upgrade Notes

No special steps required.

## Downloads

| Platform | Download |
|----------|----------|
| Linux x64 | [uft-2.9.0-linux-x64.tar.gz](link) |
| macOS Universal | [UFT-2.9.0.dmg](link) |
| Windows x64 | [uft-2.9.0-win64.zip](link) |

## Checksums

```
SHA256 (uft-2.9.0-linux-x64.tar.gz) = abc123...
SHA256 (UFT-2.9.0.dmg) = def456...
SHA256 (uft-2.9.0-win64.zip) = ghi789...
```
```

---

## 5. Post-Release

### 5.1 Verification

- [ ] Binaries heruntergeladen und getestet
- [ ] Checksums verifiziert
- [ ] GitHub Release korrekt angezeigt
- [ ] Dokumentation deployed

### 5.2 Announcement

- [ ] Discord/Slack Announcement
- [ ] Forum Post
- [ ] Social Media (optional)
- [ ] Blog Post (für Major Releases)

### 5.3 Monitoring

- [ ] Bug-Reports beobachten (erste 48h kritisch)
- [ ] Performance-Regression prüfen
- [ ] Hotfix-Bereitschaft

---

## 6. Hotfix-Prozess

Bei kritischen Bugs nach Release:

```bash
# 1. Hotfix-Branch von Tag
git checkout v2.9.0
git checkout -b hotfix/v2.9.1

# 2. Fix implementieren
# ... commit ...

# 3. Release wie oben
git tag -a v2.9.1 -m "Hotfix: Critical bug X"
```

---

## 7. Rollback

Bei schwerwiegenden Problemen:

1. GitHub Release als "Pre-release" markieren
2. Vorherige Version prominent verlinken
3. Bug analysieren und Hotfix vorbereiten
4. Kommunikation an User

---

## 8. Verantwortlichkeiten

| Rolle | Verantwortung |
|-------|---------------|
| Release Manager | Koordination, Checkliste |
| Lead Developer | Code-Review, Final Approval |
| QA Team | Testing, Verification |
| DevOps | Build, Deploy |
| Documentation | Release Notes, Docs |

---

**DOKUMENT ENDE**
