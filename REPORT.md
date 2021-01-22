
# sshell

ECS150 Project #1

In our design, our goal was to set up all the needed variables given from the
user before calling execvp(). To do this, we created a struct named Command to
hold all of the information that we needed. It held the full line that the user
inputted in order to be able to output after the command down the line. It also
held the number of arguments, the number of "|", and each word separated by
spaces into an array, terminated by a NULL. Then, our code was designed to
handle the 3 possible cases: a case where we must do piping and redirects, a
case where we only need piping, and a case where neither piping nor redirects
are used. Because of this, we intentionally designed our redirect function and
piping function to not make any changes unless a ">" or "|" was present.
Because of the way our piping function was defined, for each "|" that was
found, another child was called to execute the process, and the output was
passed through a pipe. However, we also passed the exit codes of the children
through another pipe, as we needed all the exit codes to be printed out at the
end.

We created the functions addSpace() and addSpaceAround() in order to account
for situations where no whitespaces are given between a ">" or "|" and the
surrounding arguments. In doing so, we had to assume that adding the extra
whitespaces would not force the given line to exceed the 512 character limit.

In the beginning of our testing phase, we mainly utilized CLion's debug mode,
along with having print statements in various places, in order to find and fix
bugs that appeared in our code. However, this stopped working as we got closer
towards the end, as using dup2() to change stdout to another director made
printing very harmful. Because of this, debugging became much more difficult,
as the debug mode also does not follow children, only the original parent that
runs the entire process. We were only able to see the changes that the children
create, and we had to figure out what was happening where by going line by line
through the child code.

Throughout the project, we reference the man pages of various commands, such as
exec() and its family processes, read() and write(), strtok(), strcmp(),
strncat(), and others. We also referenced the slides given in lecture and the
class Piazza page when we had doubts about logic or specifics.
