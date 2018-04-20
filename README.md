# Ramboum
un petit jeux de voiture arduino initial D style

## Pin

```
digital
```

- 0  RX1: LCD D7
- 1  TX1: LCD D6
- 4  DIG: LCD E
- 5  DIG: LCD RS
- 6  DIG: LED ROAD R
- 7  DIG: LED ROAD M
- 8  DIG: LED ROAD L
- 9  DIG: LED PLAYER L
- 10 DIG: LED PLAYER M
- 11 DIG: LED PLAYER R
- 12 DIG: BTN DOWN
- 13 DIG: BTN UP

```
analog
```

- A5 ANA: KNOCK L
- A4 ANA: KNOCK M
- A3 ANA: KNOCK R
- A2 ANA: MICROPHONE
- A1 ANA: LCD D5
- A0 ANA: LCD D4

```
i2c
```

- SCL: GYROSCOPE
- SDA: GYROSCOPE

```
power
```

- 3.3V: GYROSCOPE
- 5V: ALL
- GND: ALL

## Librairies

- I2Cdev
- LiquidCrystal
- MPU6050
- Wire

## Objectif

inspiré des anciens jeux d'arcade, l'objectif est d'éviter les voitures tout en allant le plus vite possible afin d'obtenir le meilleur score.

- Contrôle du volant au gyroscope
- Accélération au volume sonore
- Gestion du changement de vitesse

## Modes

SinglePlayer : voiture aléatoire sur le circuit

MultiPlayer : voiture positionné via les boutons
