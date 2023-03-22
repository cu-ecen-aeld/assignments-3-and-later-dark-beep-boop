
if [ $# -lt 2 ]; then
	printf 'Usage: %s <filesdir> <searchstr>\n' $0
	exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d $filesdir ]; then
	printf "%s directory doesn't exist or isn't accessible\n" $filesdir
	exit 1
fi

filecount=`find $filesdir -type f | wc -l`
linecount=`grep -R $searchstr $filesdir | wc -l`

printf 'The number of files are %d and the number of matching lines are %d\n' $filecount $linecount

