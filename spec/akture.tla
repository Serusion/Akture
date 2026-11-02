------------------------------- MODULE akture -------------------------------
(***************************************************************************)
(* The master, the greeter and the session of Akture, and the messages    *)
(* between master and greeter.                                             *)
(*                                                                         *)
(* The greeter may crash at any time and may be taken over, in which case *)
(* it sends any message at any time.  The master may crash as well.  PAM  *)
(* is trusted: it decides nondeterministically whether an answer is good. *)
(*                                                                         *)
(* Checked properties (akture.cfg):                                        *)
(*   NoSessionWithoutAuth  a session only runs after PAM accepted the      *)
(*                         attempt it was started for;                     *)
(*   AuthEnds              an authentication always ends, whatever the     *)
(*                         greeter does;                                   *)
(*   GreeterBack           an idle master always brings the greeter back. *)
(***************************************************************************)
EXTENDS Naturals, Sequences

CONSTANT MaxQueue           \* capacity of each direction of the socketpair

ToMaster == {"BEGIN_AUTH", "RESPONSE", "CANCEL", "POWER", "GARBAGE"}
ToGreeter == {"PROMPT", "RESULT_OK", "RESULT_FAIL"}

VARIABLES
    mstate,     \* "idle", "auth", "session" or "dead"
    pam,        \* "none", "asking", "ok", "fail": PAM in the current attempt
    greeter,    \* "down" or "up"
    gstate,     \* honest greeter: "input", "wait" or "cancel"
    g2m, m2g,   \* the two directions of the socketpair
    session,    \* "none" or "running"
    authed      \* ghost: PAM accepted the current attempt

vars == <<mstate, pam, greeter, gstate, g2m, m2g, session, authed>>

TypeOK ==
    /\ mstate \in {"idle", "auth", "session", "dead"}
    /\ pam \in {"none", "asking", "ok", "fail"}
    /\ greeter \in {"down", "up"}
    /\ gstate \in {"input", "wait", "cancel"}
    /\ g2m \in Seq(ToMaster) /\ Len(g2m) <= MaxQueue
    /\ m2g \in Seq(ToGreeter) /\ Len(m2g) <= MaxQueue
    /\ session \in {"none", "running"}
    /\ authed \in BOOLEAN

Init ==
    /\ mstate = "idle"
    /\ pam = "none"
    /\ greeter = "down"
    /\ gstate = "input"
    /\ g2m = <<>>
    /\ m2g = <<>>
    /\ session = "none"
    /\ authed = FALSE

\* The messages the master accepts in each state (akstate_accepts()).
Accepts(s, m) ==
    \/ s = "idle" /\ m \in {"BEGIN_AUTH", "POWER", "CANCEL"}
    \/ s = "auth" /\ m \in {"RESPONSE", "CANCEL"}

\* Killing the greeter closes the socketpair: both queues are lost.
GreeterGone ==
    /\ greeter' = "down"
    /\ gstate' = "input"
    /\ g2m' = <<>>
    /\ m2g' = <<>>

Send(q, m) == IF Len(q) < MaxQueue THEN Append(q, m) ELSE q

(***************************************************************************)
(* Master                                                                  *)
(***************************************************************************)

MStartGreeter ==
    /\ mstate = "idle"
    /\ greeter = "down"
    /\ greeter' = "up"
    /\ gstate' = "input"
    /\ UNCHANGED <<mstate, pam, g2m, m2g, session, authed>>

\* A message the current state does not accept: kill the greeter, and
\* abort PAM if it was running.
MReject ==
    /\ mstate \in {"idle", "auth"}
    /\ greeter = "up"
    /\ g2m /= <<>>
    /\ ~Accepts(mstate, Head(g2m))
    /\ GreeterGone
    /\ IF mstate = "auth"
         THEN pam' = "fail"
         ELSE UNCHANGED pam
    /\ UNCHANGED <<mstate, session, authed>>

MBegin ==
    /\ mstate = "idle"
    /\ greeter = "up"
    /\ g2m /= <<>> /\ Head(g2m) = "BEGIN_AUTH"
    /\ g2m' = Tail(g2m)
    /\ mstate' = "auth"
    /\ pam' = "asking"
    /\ m2g' = Send(m2g, "PROMPT")
    /\ UNCHANGED <<greeter, gstate, session, authed>>

\* An answer; PAM decides on it.
MResponse ==
    /\ mstate = "auth"
    /\ greeter = "up"
    /\ g2m /= <<>> /\ Head(g2m) = "RESPONSE"
    /\ g2m' = Tail(g2m)
    /\ IF pam = "asking"
         THEN \E ok \in BOOLEAN :
                /\ pam' = IF ok THEN "ok" ELSE "fail"
                /\ authed' = ok
         ELSE UNCHANGED <<pam, authed>>
    /\ UNCHANGED <<mstate, greeter, gstate, m2g, session>>

MCancel ==
    /\ mstate \in {"idle", "auth"}
    /\ greeter = "up"
    /\ g2m /= <<>> /\ Head(g2m) = "CANCEL"
    /\ g2m' = Tail(g2m)
    /\ IF mstate = "auth" /\ pam = "asking"
         THEN pam' = "fail"
         ELSE UNCHANGED pam
    /\ UNCHANGED <<mstate, greeter, gstate, m2g, session, authed>>

MPower ==
    /\ mstate = "idle"
    /\ greeter = "up"
    /\ g2m /= <<>> /\ Head(g2m) = "POWER"
    /\ g2m' = Tail(g2m)
    /\ UNCHANGED <<mstate, pam, greeter, gstate, m2g, session, authed>>

\* No answer in time, or the greeter died while PAM was asking.
MAbort ==
    /\ mstate = "auth"
    /\ pam = "asking"
    /\ pam' = "fail"
    /\ UNCHANGED <<mstate, greeter, gstate, g2m, m2g, session, authed>>

\* PAM is done.  Success stops the greeter and starts the session.
MFinish ==
    /\ mstate = "auth"
    /\ pam \in {"ok", "fail"}
    /\ pam' = "none"
    /\ IF pam = "ok"
         THEN /\ mstate' = "session"
              /\ session' = "running"
              /\ GreeterGone
              /\ UNCHANGED authed
         ELSE /\ mstate' = "idle"
              /\ m2g' = IF greeter = "up" THEN Send(m2g, "RESULT_FAIL")
                                         ELSE m2g
              /\ authed' = FALSE
              /\ UNCHANGED <<session, greeter, gstate, g2m>>

MCrash ==
    /\ mstate /= "dead"
    /\ mstate' = "dead"
    /\ pam' = "none"
    /\ GreeterGone
    /\ UNCHANGED <<session, authed>>

\* Once the master is dead and no session is left, nothing happens any
\* more: the console is left to getty.  This is the only final state;
\* TLC's deadlock check shows that there is no other.
Terminated ==
    /\ mstate = "dead"
    /\ session = "none"
    /\ UNCHANGED vars

(***************************************************************************)
(* Session                                                                 *)
(***************************************************************************)

SessionEnd ==
    /\ session = "running"
    /\ session' = "none"
    /\ authed' = FALSE
    /\ mstate' = IF mstate = "session" THEN "idle" ELSE mstate
    /\ UNCHANGED <<pam, greeter, gstate, g2m, m2g>>

(***************************************************************************)
(* Greeter: honest behaviour, crashes, and anything at all                 *)
(***************************************************************************)

GSubmit ==
    /\ greeter = "up"
    /\ gstate = "input"
    /\ g2m' = Send(g2m, "BEGIN_AUTH")
    /\ gstate' = "wait"
    /\ UNCHANGED <<mstate, pam, greeter, m2g, session, authed>>

GCancel ==
    /\ greeter = "up"
    /\ gstate = "wait"
    /\ g2m' = Send(g2m, "CANCEL")
    /\ gstate' = "cancel"
    /\ UNCHANGED <<mstate, pam, greeter, m2g, session, authed>>

GRecv ==
    /\ greeter = "up"
    /\ m2g /= <<>>
    /\ m2g' = Tail(m2g)
    /\ CASE Head(m2g) = "PROMPT" ->
              /\ g2m' = IF gstate = "wait" THEN Send(g2m, "RESPONSE")
                                          ELSE g2m
              /\ UNCHANGED gstate
         [] Head(m2g) = "RESULT_FAIL" ->
              /\ gstate' = "input"
              /\ UNCHANGED g2m
         [] OTHER ->
              UNCHANGED <<gstate, g2m>>
    /\ UNCHANGED <<mstate, pam, greeter, session, authed>>

GCrash ==
    /\ greeter = "up"
    /\ GreeterGone
    /\ UNCHANGED <<mstate, pam, session, authed>>

GEvil ==
    /\ greeter = "up"
    /\ \E m \in ToMaster : g2m' = Send(g2m, m)
    /\ UNCHANGED <<mstate, pam, greeter, gstate, m2g, session, authed>>

(***************************************************************************)

Master == MStartGreeter \/ MReject \/ MBegin \/ MResponse \/ MCancel
          \/ MPower \/ MAbort \/ MFinish

Next ==
    \/ Master \/ MCrash \/ Terminated
    \/ SessionEnd
    \/ GSubmit \/ GCancel \/ GRecv \/ GCrash \/ GEvil

\* The master keeps working; MAbort stands for its timeout.  Nothing is
\* assumed about the greeter or the session.
Spec ==
    /\ Init
    /\ [][Next]_vars
    /\ WF_vars(MStartGreeter) /\ WF_vars(MReject) /\ WF_vars(MBegin)
    /\ WF_vars(MResponse) /\ WF_vars(MCancel) /\ WF_vars(MPower)
    /\ WF_vars(MAbort) /\ WF_vars(MFinish)

NoSessionWithoutAuth ==
    /\ session = "running" => authed
    /\ mstate = "session" => authed

AuthEnds == (mstate = "auth") ~> (mstate \in {"idle", "session", "dead"})

GreeterBack ==
    (mstate = "idle" /\ greeter = "down") ~> (greeter = "up" \/ mstate = "dead")

=============================================================================
