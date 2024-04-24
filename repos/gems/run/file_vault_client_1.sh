#!/bin/bash
echo "Hallo Welt!" > file_vault/file_1
echo "--- file_1 (new) ---"
cat file_vault/file_1
echo "Ein zweiter Test." > file_vault/file_1
echo "--- file_1 (modified) ---"
cat file_vault/file_1
echo "--- ls ---"
ls -la file_vault/
mkdir file_vault/dir_1
echo "Eine weitere Datei." > file_vault/dir_1/file_2
echo "Mit mehr Inhalt." >> file_vault/dir_1/file_2
echo "--- dir_1/file_2 (new) ---"
cat file_vault/dir_1/file_2
echo "Und Sonderzeichen: /.:($)=%!" >> file_vault/dir_1/file_2
echo "--- dir_1/file_2 (modified) ---"
cat file_vault/dir_1/file_2
echo "--- ls ---"
ls -la file_vault/
echo "--- ls dir_1 ---"
ls -la file_vault/dir_1
exit 0
