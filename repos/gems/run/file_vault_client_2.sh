#!/bin/bash
echo "--- ls ---"
ls -la file_vault/
echo "--- ls dir_1 ---"
ls -la file_vault/dir_1
echo "--- file_1 ---"
cat file_vault/file_1
echo "--- dir_1/file_2 ---"
cat file_vault/dir_1/file_2
echo "Hallo Welt!" >> file_vault/file_1
echo "Hallo Mond..." > file_vault/dir_1/file_2
echo "... und Erde! 123" >> file_vault//dir_1/file_2
mkdir file_vault/dir_2
echo "Eine frisch erzeugte Datei." > file_vault/dir_2/file_3
echo "--- file_1 (modified) ---"
cat file_vault/file_1
echo "--- dir_1/file_2 (modified) ---"
cat file_vault/dir_1/file_2
echo "--- dir_2/file_3 (new) ---"
cat file_vault/dir_2/file_3
exit 0
