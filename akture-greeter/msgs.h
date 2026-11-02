/*-
 * Copyright (c) 2026 Serusion
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MSGS_H_
#define _MSGS_H_

/*
 * Every text shown by the greeter, in one place.
 */
#define MSG_PASSWORD		"パスワード"
#define MSG_USERNAME		"ユーザー名"
#define MSG_ANSWER		"応答"
#define MSG_OTHERUSER		"別のユーザー"
#define MSG_ENTERUSER		"ユーザー名を入力"
#define MSG_CAPSLOCK		"Caps Lockがオンです"
#define MSG_BADPASS		"パスワードが違います"
#define MSG_SESSIONFAIL		"セッションを開始できませんでした"
#define MSG_SHUTDOWN		"シャットダウン"
#define MSG_REBOOT		"再起動"

/* Date under the clock: month, day, weekday */
#define MSG_DATE_FMT		"%d月%d日 %s"
#define MSG_WEEKDAYS							\
	"日曜日", "月曜日", "火曜日", "水曜日", "木曜日", "金曜日", "土曜日"

#endif /* !_MSGS_H_ */
