(* Copyright (C) 1999-2005 Henry Cejtin, Matthew Fluet, Suresh
 *    Jagannathan, and Stephen Weeks.
 * Copyright (C) 1997-2000 NEC Research Institute.
 *
 * MLton is released under a BSD-style license.
 * See the file MLton-LICENSE for details.
 *)

type int = Int.t
type word = Word.t

signature RUNTIME_STRUCTS =
   sig
   end

signature RUNTIME =
   sig
      include RUNTIME_STRUCTS

      structure GCField:
         sig
            datatype t =
               CanHandle
             | CardMap
             | CurrentThread
             | CurSourceSeqsIndex
             | ExnStack
             | Frontier (* The place where the next object is allocated. *)
             | Limit (* frontier + heapSize - LIMIT_SLOP *)
             | LimitPlusSlop (* frontier + heapSize *)
             | MaxFrameSize
             | SignalIsPending
             | StackBottom
             | StackLimit (* Must have StackTop <= StackLimit *)
             | StackTop (* Points at the next available byte on the stack. *)

            val layout: t -> Layout.t
            val offset: t -> Bytes.t (* Field offset in struct GC_state. *)
            val setOffsets: {canHandle: Bytes.t,
                             cardMap: Bytes.t,
                             currentThread: Bytes.t,
                             curSourceSeqsIndex: Bytes.t,
                             exnStack: Bytes.t,
                             frontier: Bytes.t,
                             limit: Bytes.t,
                             limitPlusSlop: Bytes.t,
                             maxFrameSize: Bytes.t,
                             signalIsPending: Bytes.t,
                             stackBottom: Bytes.t,
                             stackLimit: Bytes.t,
                             stackTop: Bytes.t} -> unit
            val setSizes: {canHandle: Bytes.t,
                           cardMap: Bytes.t,
                           currentThread: Bytes.t,
                           curSourceSeqsIndex: Bytes.t,
                           exnStack: Bytes.t,
                           frontier: Bytes.t,
                           limit: Bytes.t,
                           limitPlusSlop: Bytes.t,
                           maxFrameSize: Bytes.t,
                           signalIsPending: Bytes.t,
                           stackBottom: Bytes.t,
                           stackLimit: Bytes.t,
                           stackTop: Bytes.t} -> unit
            val size: t -> Bytes.t (* Field size in struct GC_state. *)
            val toString: t -> string
         end
      structure RObjectType:
         sig
            datatype t =
               Array of {hasIdentity: bool,
                         bytesNonObjptrs: Bytes.t,
                         numObjptrs: int}
             | Normal of {hasIdentity: bool,
                          bytesNonObjptrs: Bytes.t,
                          numObjptrs: int}
             | Stack
             | Weak
             | WeakGone
         end

      val allocTooLarge: Bytes.t
      val arrayLengthOffset: unit -> Bytes.t
      val arrayLengthSize: unit -> Bytes.t
      val headerOffset: unit -> Bytes.t
      val headerSize: unit -> Bytes.t
      val headerToTypeIndex: word -> int
      val labelSize: unit -> Bytes.t
      val limitSlop: Bytes.t
      val maxFrameSize: Bytes.t
      val cpointerSize: unit -> Bytes.t
      val objptrSize: unit -> Bytes.t
      val typeIndexToHeader: int -> word
   end
