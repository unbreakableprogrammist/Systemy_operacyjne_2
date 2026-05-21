#!/bin/bash

# Jeśli użytkownik podał port jako argument, używamy go. W przeciwnym razie domyślnie 8080.
PORT=${1:-8080}
IP="127.0.0.1"

echo "Rozpoczynam wysyłanie meldunków testowych do serwera ($IP:$PORT)..."
echo "----------------------------------------------------------------"

# 1. Poprawne meldunki
echo "Wysyłam: 12 45 1 Stara Gwardia"
echo "12 45 1 Stara Gwardia" | nc -w0 -u $IP $PORT
sleep 0.5

echo "Wysyłam: 88 10 0 Pruskie Rezerwy"
echo "88 10 0 Pruskie Rezerwy" | nc -w0 -u $IP $PORT
sleep 0.5

echo "Wysyłam: 50 50 1 Artyleria Konna"
echo "50 50 1 Artyleria Konna" | nc -w0 -u $IP $PORT
sleep 0.5

# 2. Błędne meldunki (test odporności serwera)
echo "Wysyłam błędne współrzędne: 100 200 5 Błędny Oddział"
echo "100 200 5 Błędny Oddział" | nc -w0 -u $IP $PORT
sleep 0.5

echo "Wysyłam całkowicie zły format tekstu"
echo "To jest losowy tekst bez sensu" | nc -w0 -u $IP $PORT
sleep 0.5

echo "----------------------------------------------------------------"
echo "Wysyłanie zakończone. Sprawdź terminal serwera!"