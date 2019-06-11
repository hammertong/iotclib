if [ -f $1 ]
then
  openssl x509 -noout -text -in $1 | grep "After\|Subject:\|Public-Key" | sed -e 's/^[ \t]*//;s/[ \t]*$//'
else
  echo "Usage:"
  echo "$0 <PEM file>"
fi

