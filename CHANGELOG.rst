
Changelog
=========

`v1.5.4 <//github.com/smlgit/fpbinary/releases/tag/v1.5.4>`_
----------------------------------------------------------------

**Enhancements:**

* Build macOS and Windows binaries for Python 3.10 and 3.11.

  * Note that Win32 binaries for 3.10 and later will not be supported.

**Bug fixes:**

* Add support for Numpy. `#9 <//github.com/smlgit/fpbinary/issues/9>`_


`v1.5.3 <//github.com/smlgit/fpbinary/releases/tag/v1.5.3>`_
----------------------------------------------------------------

**Enhancements:**

* Build macOS and Windows binaries for Python 3.9.0.


`v1.5.2 <//github.com/smlgit/fpbinary/releases/tag/v1.5.2>`_
----------------------------------------------------------------

**Enhancements:**

* Add support for macOS installation binaries. `#19 <//github.com/smlgit/fpbinary/issues/19>`_
* Modify the Overflow and Rounding enums to be static classes. `#18 <//github.com/smlgit/fpbinary/issues/18>`_


`v1.5.1 <//github.com/smlgit/fpbinary/releases/tag/v1.5.1>`_
----------------------------------------------------------------

**Enhancements:**

* Add PyPi and readthedocs support. `#16 <//github.com/smlgit/fpbinary/issues/16>`_

**Bug fixes:**


* Unpickling an FpBinary instance that was created on a larger word length machine could theoretically produce the wrong value `#15 <//github.com/smlgit/fpbinary/issues/15>`_
* FpBinary int() method may return incorrect value when running 32 bit python on a 64 bit machine `#14 <//github.com/smlgit/fpbinary/issues/14>`_
* Division on FpBinary objects may cause a crash `#13 <//github.com/smlgit/fpbinary/issues/13>`_

`v1.4 <//github.com/smlgit/fpbinary/releases/tag/v1.4>`_
------------------------------------------------------------

**Bug fixes:**


* Installation on Windows 10. `#12 <//github.com/smlgit/fpbinary/issues/12>`_

`v1.3 <//github.com/smlgit/fpbinary/releases/tag/v1.3>`_
------------------------------------------------------------

**Enhancements:**


* Added support for pickling `#10 <//github.com/smlgit/fpbinary/issues/10>`_

**Bug fixes:**


* Copying FpBinarySwitchable objects may result in the wrong min_value or max_value. `#11 <//github.com/smlgit/fpbinary/issues/11>`_
