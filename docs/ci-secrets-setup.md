# eAI CI/CD Secrets & Android Setup Guide

## Required Secrets: NONE

The default eAI CI configuration works without any secrets. The Android NDK is
downloaded automatically by `nttld/setup-ndk@v1`, and the Android emulator uses
the pre-installed SDK on GitHub's `ubuntu-latest` runners.

## Optional Secrets (Enhanced Testing)

### Firebase Test Lab (physical device testing)

For running HAL tests on real Android hardware via Firebase Test Lab:

1. Create a Google Cloud project and enable the Firebase Test Lab API
2. Create a service account with `Firebase Test Lab Admin` role
3. Export the service account key as JSON
4. Add these secrets to your repository:

| Secret Name | Value | Where to Get |
|-------------|-------|--------------|
| `GOOGLE_CLOUD_PROJECT` | Your GCP project ID | GCP Console → Project Settings |
| `GOOGLE_CLOUD_SA_KEY` | Service account JSON key (base64) | `base64 < sa-key.json` |

```bash
# Encode the service account key:
base64 -w0 < firebase-sa-key.json | pbcopy
# Paste into GitHub → Settings → Secrets → GOOGLE_CLOUD_SA_KEY
```

### Android Signing (release APKs)

Only needed if distributing Android test APKs:

| Secret Name | Value |
|-------------|-------|
| `ANDROID_KEYSTORE_BASE64` | Base64-encoded `.jks` keystore |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Key alias name |
| `ANDROID_KEY_PASSWORD` | Key password |

### Docker Registry (for caching CI images)

To speed up Docker-based HAL testing by caching built images:

| Secret Name | Value |
|-------------|-------|
| `DOCKER_REGISTRY` | e.g. `ghcr.io/embeddedos-org` |
| `DOCKER_USERNAME` | GitHub username or `${{ github.actor }}` |
| `DOCKER_PASSWORD` | GitHub PAT with `write:packages` scope |

Note: GitHub Container Registry (ghcr.io) can use the built-in `GITHUB_TOKEN`
with `packages: write` permission instead of a separate secret.

## How to Add Secrets

1. Go to your GitHub repository
2. Navigate to **Settings → Secrets and variables → Actions**
3. Click **New repository secret**
4. Enter the secret name and value

## Verifying the Setup

After adding secrets, run the workflow manually:

```
GitHub → Actions → Cross-Platform HAL Tests → Run workflow
```

The Android emulator job will appear as a separate check. Firebase Test Lab
(if configured) will show as `hal-android-ftl`.
