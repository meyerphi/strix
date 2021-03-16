// stripped down version of cfile crate, using the generated bindings

use std::io;
use std::mem;
use std::os::raw::{c_int, c_ulong, c_void};

use crate::bindings;

pub type FilePtr = *mut bindings::FILE;

/// A reference to an open stream on the filesystem.
pub struct CFile {
    ptr: FilePtr,
}

impl Drop for CFile {
    fn drop(&mut self) {
        unsafe { bindings::fclose(self.as_ptr()) };
    }
}

/// open a temporary file as a read/write stream
pub fn tmpfile() -> io::Result<CFile> {
    unsafe {
        let p = bindings::tmpfile();

        if p.is_null() {
            Err(io::Error::last_os_error())
        } else {
            Ok(CFile::from_ptr(p))
        }
    }
}

impl io::Read for CFile {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.read_slice(buf)
    }
}

impl io::Seek for CFile {
    fn seek(&mut self, pos: io::SeekFrom) -> io::Result<u64> {
        let ret = unsafe {
            match pos {
                io::SeekFrom::Start(off) => {
                    bindings::fseek(self.as_ptr(), off as i64, bindings::SEEK_SET as c_int)
                }
                io::SeekFrom::End(off) => {
                    bindings::fseek(self.as_ptr(), off, bindings::SEEK_END as c_int)
                }
                io::SeekFrom::Current(off) => {
                    bindings::fseek(self.as_ptr(), off, bindings::SEEK_CUR as c_int)
                }
            }
        };

        if ret != 0 {
            if let Some(err) = self.last_error() {
                return Err(err);
            }
        }

        self.position()
    }
}

impl io::Write for CFile {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.write_slice(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        if unsafe { bindings::fflush(self.as_ptr()) } != 0 {
            if let Some(err) = self.last_error() {
                return Err(err);
            }
        }

        Ok(())
    }
}

impl CFile {
    pub fn from_ptr(ptr: FilePtr) -> CFile {
        CFile { ptr }
    }

    pub fn as_ptr(&self) -> FilePtr {
        self.ptr
    }

    /// returns the current position of the stream.
    pub fn position(&self) -> io::Result<u64> {
        let off = unsafe { bindings::ftell(self.as_ptr()) };

        if off < 0 {
            if let Some(err) = self.last_error() {
                return Err(err);
            }
        }

        Ok(off as u64)
    }

    /// tests the error indicator for the stream
    #[inline]
    fn errno(&self) -> i32 {
        unsafe { bindings::ferror(self.as_ptr()) }
    }

    /// get the last error of the stream
    fn last_error(&self) -> Option<io::Error> {
        let errno = self.errno();

        if errno != 0 {
            return Some(io::Error::from_raw_os_error(errno));
        }

        let err = io::Error::last_os_error();

        match err.raw_os_error() {
            Some(errno) if errno != 0 => Some(err),
            _ => None,
        }
    }

    /// Reads n elements of data, return the number of items read.
    fn read_slice<T: Sized>(&mut self, elements: &mut [T]) -> io::Result<usize> {
        if elements.is_empty() {
            return Ok(0);
        }

        let read = unsafe {
            bindings::fread(
                elements.as_mut_ptr() as *mut c_void,
                mem::size_of::<T>() as c_ulong,
                elements.len() as c_ulong,
                self.as_ptr(),
            ) as usize
        };

        if let Some(err) = self.last_error() {
            if read == 0 {
                return Err(err);
            }
        }

        Ok(read)
    }

    /// Writes n elements of data, return the number of items written.
    fn write_slice<T: Sized>(&mut self, elements: &[T]) -> io::Result<usize> {
        if elements.is_empty() {
            return Ok(0);
        }

        let wrote = unsafe {
            bindings::fwrite(
                elements.as_ptr() as *const c_void,
                mem::size_of::<T>() as c_ulong,
                elements.len() as c_ulong,
                self.as_ptr(),
            ) as usize
        };

        if let Some(err) = self.last_error() {
            if wrote == 0 {
                return Err(err);
            }
        }

        Ok(wrote)
    }
}
