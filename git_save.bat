@echo off

set /p commitMessage="Commit message: "

git add .
git commit -m "%commitMessage%"
git push origin
