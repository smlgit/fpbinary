# Changelog

## [Unreleased](https://github.com/smlgit/fpbinary/tree/HEAD)
**Bug fixes:**

- FpBinary int() method may return incorrect value when running 32 bit python on a 64 bit machine [#14](//github.com/smlgit/fpbinary/issues/14)
- Division on FpBinary objects may cause a crash [#13](//github.com/smlgit/fpbinary/issues/13)

## [v1.4](//github.com/smlgit/fpbinary/releases/tag/v1.4)
**Bug fixes:**

- Installation on Windows 10. [#12](//github.com/smlgit/fpbinary/issues/12)

## [v1.3](//github.com/smlgit/fpbinary/releases/tag/v1.3)

**Enhancements:**

- Added support for pickling [#10](//github.com/smlgit/fpbinary/issues/10)

**Bug fixes:**

- Copying FpBinarySwitchable objects may result in the wrong min_value or max_value. [#11](//github.com/smlgit/fpbinary/issues/11)
