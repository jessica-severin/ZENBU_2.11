=head1 NAME - EEDB::SPStream::Feature2Expression

=head1 DESCRIPTION

a simple transform which expands features into a stream of Expression objects

=head1 CONTACT

Jessica Severin <severin@gsc.riken.jp>

=head1 LICENSE

 * Software License Agreement (BSD License)
 * EdgeExpressDB [eeDB] system
 * copyright (c) 2007-2009 Jessica Severin RIKEN OSC
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Jessica Severin RIKEN OSC nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=head1 APPENDIX

The rest of the documentation details each of the object methods. Internal methods are usually preceded with a _

=cut

package EEDB::SPStream::Feature2Expression;

use strict;

use EEDB::SPStream;
our @ISA = qw(EEDB::SPStream);

#################################################
# Class methods
#################################################

sub class { return "EEDB::SPStream::Feature2Expression"; }


#################################################
# Instance methods
#################################################

sub init {
  my $self = shift;
  my @args = @_;
  $self->SUPER::init(@args);
  $self->{'_expression_buffer'} = [];
  return $self;
}


#################################################
#
# override method for subclasses which will
# do all the work
#
#################################################

sub next_in_stream {
  my $self = shift;
  if(!defined($self->source_stream)) { return undef; }

  if(scalar(@{$self->{'_expression_buffer'}}) == 0) {
    my $obj = $self->source_stream->next_in_stream;
    if(!defined($obj)) { return undef; }
    if($obj->class ne 'Feature') { return $obj; }

    $obj->expression_cache; #does lazy load if not already in memory
    my $exp_array = $obj->get_expression_array;
    foreach my $expr (@$exp_array) {
      push @{$self->{'_expression_buffer'}}, $expr;
      #invert relation so the expression needs to have the feature now
      $expr->feature($obj); 
    }
    $obj->empty_expression_cache; #to prevent memory leak
  }
  
  my $expression = shift @{$self->{'_expression_buffer'}};
  return $expression;
}


#
#################################################
#

sub display_desc {
  my $self = shift;
  my $str = sprintf("Feature2Expression");
  return $str;
}

sub xml_start {
  my $self = shift;
  my $str = $self->SUPER::xml_start;
  return $str;
}

sub _init_from_xmltree {
  my $self = shift;
  my $xmlTree = shift;  #a hash tree generated by XML::TreePP
  
  unless($xmlTree->{'-module'} eq "EEDB::SPStream::Feature2Expression") { return undef; }  
  return $self;
}

1;

