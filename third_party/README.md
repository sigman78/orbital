# Third-party code

## NoGraphicsAPI (vendored fork)

`NoGraphicsAPI/` is a vendored copy of [sebbbi/NoGraphicsAPI](https://github.com/sebbbi/NoGraphicsAPI)
at upstream commit `8e414bd0a8010b9f721d06d470860e27aa69c071` (MIT, see `NoGraphicsAPI/LICENSE`).

The copy carries local compatibility changes: an opt-in conventional Vulkan 1.3 descriptor backend
(`NOGRAPHICSAPI_ENABLE_CONVENTIONAL_BACKEND` / `NOGRAPHICSAPI_FORCE_CONVENTIONAL_BACKEND`) for GPUs
without the experimental descriptor and address-command extensions. The public C++ API is unchanged.
See [docs/FOUNDATION.md](../docs/FOUNDATION.md) for the investigation.

`NoGraphicsAPI-compat.patch` is the full diff against the upstream commit. To regenerate it after
editing the fork:

```powershell
git clone https://github.com/sebbbi/NoGraphicsAPI.git upstream
git -C upstream checkout 8e414bd0a8010b9f721d06d470860e27aa69c071
git diff --no-index --src-prefix=a/ --dst-prefix=b/ upstream NoGraphicsAPI > NoGraphicsAPI-compat.patch
```

Vendored code keeps its upstream formatting (`NoGraphicsAPI/.clang-format`) and is excluded from
the project's clang-format and clang-tidy runs.
