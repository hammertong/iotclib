#!/bin/sh
export AUTHKEY="AAAAXtN_E_8:APA91bFLLppAdYvp4dRabFtlF-WVEISSpygEBtBQ8n2rI5-90ocKK6o4qCxU7J73DW6UI3DMLNtDoQ_6_Ue5QPcXyNzIYobFi5PkZWjP05PTLV3z7hn4jgkNo9FSH_XnTrGHaOd1a3fU"
export TOKEN="d1FD7uWwG5o:APA91bEXl14vJz5F-cLUjIYZ_DmYPyVVbSyKkPpPF5C9PY367eTtfmzJtFWdBjI-v10bzUtagGzkLmtL91LCP5Y5z2JEn6mcQddRjgsMa-86kWNpwid4uIlXrnl6j0LbWGoOO6K27eua"

curl -X POST --header "Authorization: key=$AUTHKEY" --Header "Content-Type: application/json" https://fcm.googleapis.com/fcm/send -d "{\"to\":\"$TOKEN\",\"notification\":{\"body\":\"ENTER YOUR MESSAGE HERE\"}}"

