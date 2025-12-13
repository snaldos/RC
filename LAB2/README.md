# FTP Client

This is a simple FTP client written in C that downloads files from an FTP server using passive mode.

## Compilation

To compile the program, run:

```
make
```

This will create an executable named `download`.

## Running

To run the program, provide an FTP URL as an argument:

```
./download ftp://[user:password@]host/path/to/file
```

- For anonymous login, you can omit the `user:password@` part.
- The program will download the file to the current directory with the filename from the URL.


## Examples

You can test the FTP download application with public FTP servers available on the Internet. The following list contains examples of URLs that you can use to test both the anonymous mode and user-password authentication:

- ftp://ftp.up.pt/pub/archlinux/archive/iso/arch-0.8-base-i686.iso
- ftp://demo:password@test.rebex.net/readme.txt
- ftp://anonymous:anonymous@ftp.bit.nl/speedtest/100mb.bin

