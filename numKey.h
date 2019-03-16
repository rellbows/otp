/*
 * Filename: keyNum.h
 * Description: Header file that holds num to key and key to num arrays
 * for easy conversion while encrypting/decrypting in the otp_enc_d
 * / otp_dec_d scripts.
 * Author: Ryan Ellis
 * Class: CS344 - Operating Systems
 */

#ifndef KEYNUM_H
#define KEYNUM_H

int keyToNum(char key);
char numToKey(int keyIndex);
int getMaxKeyIndex();

#endif
