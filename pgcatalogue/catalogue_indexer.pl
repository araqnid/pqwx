#!/usr/bin/perl -w

require 5;
use strict;
use warnings;
use utf8;
use Data::Dumper;
use Time::HiRes qw(gettimeofday tv_interval);

{ package Document; use base qw(Class::Accessor::Faster); Document->mk_accessors(qw|id type symbol|); sub new { my $class = shift; my $this = bless([], $class); $this->id(shift); $this->type(shift); $this->symbol(shift); return $this; } }
{ package Occurrence; use base qw(Class::Accessor::Faster); Occurrence->mk_accessors(qw|document_id term_id position|); sub new { my $class = shift; my $this = bless([], $class); $this->document_id(shift); $this->term_id(shift); $this->position(shift); return $this; } }
{ package Result; use base qw(Class::Accessor::Faster); Result->mk_accessors(qw|document score|); sub new { my $class = shift; my $this = bless([], $class); $this->document(shift); $this->score(shift); return $this; } }

sub split_studly_caps {
    my $text = shift;
    my @parts;
    if ($text =~ s/^([^A-Z]+)//) {
	push @parts, $1;
    }
    while ($text =~ s/^([A-Z][^A-Z]*)//) {
	push @parts, $1;
    }
    push @parts, $text if ($text);
    return @parts;
}

sub analyse {
    my $symbol = shift;
    return map { lc($_) } map { split_studly_caps($_) } split(/[^A-Za-z0-9]+/, $symbol);
}

our @DOCUMENTS;
our %PREFIXES;
our %TERMS_INDEX;
our @TERMS;
our @OCCURRENCES;
our @DOCUMENT_TERMS;

sub index_entity {
    my $entity_id = shift;
    my $entity_type = shift;
    my $entity_symbol = shift;

    push @DOCUMENTS, new Document($entity_id, $entity_type, $entity_symbol);
    my $document_id = int($#DOCUMENTS);

    my @tokens = analyse($entity_symbol);

    for my $pos (0..$#tokens) {
	my $token = $tokens[$pos];
	my $term_id = $TERMS_INDEX{$token};
	if (!defined($term_id)) {
	    push @TERMS, $token;
	    $term_id = $#TERMS;
	    $TERMS_INDEX{$token} = $term_id;
	    # Perl-ish: index prefixes using a hash table
	    # really, should just have a tree of tokens linking to term IDs
	    for my $len (1..length($token)) {
		my $prefix = substr($token, 0, $len);
		push @{$PREFIXES{$prefix}}, $term_id;
	    }
	}

	push @{$DOCUMENT_TERMS[$document_id]}, $term_id;
	push @{$OCCURRENCES[$term_id]}, new Occurrence($document_id, $term_id, $pos);
    }
}

my $indexing_start = [gettimeofday];

while (<STDIN>) {
    chomp;
    my($id, $type, $symbol) = split(/\|/);
    index_entity(int($id), $type, $symbol);
}

my $indexing_finished = [gettimeofday];

print "** Indexed ".scalar(@TERMS)." terms of ".scalar(@DOCUMENTS)." documents in ".sprintf("%.3f", tv_interval($indexing_start, $indexing_finished))." seconds\n";

sub build_type_filter {
    my $filter_spec = shift;
    my $buildfilter_started = [gettimeofday];
    $filter_spec =~ s/^\@//;
    my @docs;
    my %accept_types = map { ($_, 1) } split(/,/, $filter_spec);
    for my $i (0..$#DOCUMENTS) {
	$docs[$i] = 1 if ($accept_types{$DOCUMENTS[$i]->type});
    }
    my $buildfilter_finished = [gettimeofday];
    print "** Built types<$filter_spec> filter in ".sprintf("%.3f", tv_interval($buildfilter_started, $buildfilter_finished))." seconds\n";
    return \@docs;
}

sub build_nonsystem_filter {
    my $buildfilter_started = [gettimeofday];
    my @docs;
    for my $i (0..$#DOCUMENTS) {
	$docs[$i] = 1 if ($DOCUMENTS[$i]->symbol !~ /^(information_schema|pg_catalog)\./);
    }
    my $buildfilter_finished = [gettimeofday];
    print "** Built non-system filter in ".sprintf("%.3f", tv_interval($buildfilter_started, $buildfilter_finished))." seconds\n";
    return \@docs;
}

sub combine_filters {
    if (@_ == 2) {
	return combine_2filters(@_);
    }
    else {
	my $filter1 = shift;
	my $filter2 = shift;
	return combine_filters(combine_2filters($filter1, $filter2), @_);
    }
}

sub combine_2filters {
    my $filter1 = shift;
    my $filter2 = shift;

    my $combinefilter_started = [gettimeofday];

    my @output;
    for my $i (0..$#DOCUMENTS) {
	$output[$i] = $filter1->[$i] && $filter2->[$i];
    }

    my $combinefilter_finished = [gettimeofday];
    print "** Combined 2 filters in ".sprintf("%.3f", tv_interval($combinefilter_started, $combinefilter_finished))." seconds\n";

    return \@output;
}

sub match_terms {
    my $token = shift;
    return exists $PREFIXES{$token} ? @{$PREFIXES{$token}} : ();
}

my $nonsystem_filter = build_nonsystem_filter();
my $filter = $nonsystem_filter;
for my $input (@ARGV) {
    if ($input =~ /^@/) {
	$filter = combine_filters(build_type_filter($input), $nonsystem_filter);
	next;
    }

    my $search_started = [gettimeofday];
    my @query_tokens = analyse($input);
    my @term_matches;
    for my $term (@query_tokens) {
	my %occurrences;
	for my $term_id (match_terms($term)) {
	    for my $occurrence (@{$OCCURRENCES[$term_id]}) {
		$occurrences{$occurrence->document_id . '@' . $occurrence->position} = $occurrence;
	    }
	}
	push @term_matches, \%occurrences;
    }
    my @score_docs;
    for my $first_term (values %{$term_matches[0]}) {
	my $document_id = $first_term->document_id;
	next if ($filter && !$filter->[$document_id]);
	my $position = $first_term->position;
	my @matched = ($first_term);
	for my $offset (1..$#term_matches) {
	    $position++;
	    my $occurrence = $term_matches[$offset]->{$document_id . '@' . $position};
	    if ($occurrence) {
		push @matched, $occurrence;
	    } else {
		last;
	    }
	}
	my $document = $DOCUMENTS[$document_id];
	if ($#matched == $#query_tokens) {
	    #print $document->symbol."   match: \"".join(" | ", map { $TERMS[$_->term_id] } @matched)."\" against \"".join(" | ", @query_tokens)."\"\n";
	    #print " first position: ".$first_term->position."\n";
	    #print " document terms are: ".join(" | ", @{$DOCUMENT_TERMS[$document_id]})."\n";
	    my $suffix_length = $#{$DOCUMENT_TERMS[$document_id]} - $#matched;
	    #print " trailing terms not matched: $suffix_length\n";
	    my $last_length_difference;
	    my $total_length_difference = 0;
	    for my $i (0..$#matched) {
		my $term_token = $TERMS[$matched[$i]->term_id];
		my $query_token = $query_tokens[$i];
		$last_length_difference = length($term_token) - length($query_token);
		$total_length_difference += length($term_token) - length($query_token);
	    }
	    #print " last token length difference: $last_length_difference\n";
	    #print " other tokens length difference: ".($total_length_difference-$last_length_difference)."\n";
	    my $score = - $first_term->position - $suffix_length - $last_length_difference - ($total_length_difference-$last_length_difference);
	    push @score_docs, new Result($document, $score);
	}
    }
    #print Data::Dumper->new([$input, \@query_tokens, \@term_matches], [qw|input query_tokens term_matches|])->Dump;
    for my $result (sort { $b->score <=> $a->score || $a->document->symbol cmp $b->document->symbol } @score_docs) {
	print $result->document->symbol . " " . $result->document->type . '#' . $result->document->id . " (".$result->score.")\n";
    }
    my $search_finished = [gettimeofday];
    print "** Completed search in ".sprintf("%.3f", tv_interval($search_started, $search_finished))." seconds\n";
}

# psql -F' ' -At -Ppager=off -f catalogue_driver.sql media > catalogue_input.txt
