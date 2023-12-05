#!/bin/bash
ls -la file_vault/
ls -la file_vault/dir_1
cat file_vault/file_1
cat file_vault/dir_1/file_2
echo "Ein bisschen angefügter Inhalt. %&$§(" >> file_vault/file_1
echo "Ein ganz neuer Inhalt." > file_vault/dir_1/file_2
mkdir file_vault/dir_2
echo "Und sogar eine neue Datei." >> file_vault/dir_2/file_3
echo "In einem neuen Ordner." >> file_vault/dir_2/file_3
ls -la file_vault/dir_2
cat file_vault/file_1
cat file_vault/dir_1/file_2
cat file_vault/dir_2/file_3
exit 0
